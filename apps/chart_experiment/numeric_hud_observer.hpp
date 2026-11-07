#pragma once
#include "numeric_hud_metadata.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <span>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32) && __has_include(<winrt/Windows.Media.Ocr.h>)
#define RHYTHM_EXPERIMENTAL_NUMERIC_HUD_WINRT 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Globalization.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>
#if defined(_MSC_VER)
#pragma comment(lib,"windowsapp.lib")
#endif
#else
#define RHYTHM_EXPERIMENTAL_NUMERIC_HUD_WINRT 0
#endif

namespace rhythm::experimental {
// Optional numeric diagnostics ONLY. A single producer offers actual frames;
// one worker owns OCR and rows. No full-frame leases, capture calls, clocks,
// permissions, input decisions or file writes are introduced here. Construction
// belongs before READY; finish/destruction belongs AFTER all touch release.
class NumericHudObserver {
public:
    static constexpr std::size_t score_copy_bytes=180u*40u,combo_copy_bytes=160u*72u;
private:
    enum class Slot {starting,idle,copying,queued,processing,stopped};
    const std::size_t maximum_combo_;
    NumericHudMetadataBudget budget_;
    NumericHudObserverSummary summary_;
    std::vector<NumericHudRow> rows_;
    std::array<std::uint8_t,score_copy_bytes> score_pixels_{};
    std::array<std::uint8_t,combo_copy_bytes> combo_pixels_{};
    NumericHudFrame frame_;
    ProgressBarMetadata progress_bar_;
    std::atomic<Slot> slot_{Slot::starting};
    std::atomic<bool> stopping_{false},available_{false};
    std::thread worker_;
    std::mutex wake_mutex_;
    std::condition_variable wake_;

    static void copy_ink(std::span<const std::uint8_t> rgb,std::uint32_t stride,int x,int y,int width,int height,
            std::uint8_t* output) noexcept {
        for(int row=0;row<height;++row){
            const auto* source=rgb.data()+static_cast<std::size_t>(y+row)*stride+static_cast<std::size_t>(x)*3u;
            for(int col=0;col<width;++col,source+=3){
                const auto lo=std::min({source[0],source[1],source[2]});
                const auto hi=std::max({source[0],source[1],source[2]});
                *output++=lo>=180&&hi-lo<=70?lo:std::uint8_t{0};
            }
        }
    }
    static std::int64_t elapsed_us(std::chrono::steady_clock::time_point start) noexcept {
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    }
    void clear_pixels() noexcept {score_pixels_.fill(0);combo_pixels_.fill(0);}

#if RHYTHM_EXPERIMENTAL_NUMERIC_HUD_WINRT
    using Engine=winrt::Windows::Media::Ocr::OcrEngine;
    using Bitmap=winrt::Windows::Graphics::Imaging::SoftwareBitmap;
    Bitmap bitmap(std::span<const std::uint8_t> ink,int roi_width,const std::array<int,4>& local_crop,bool inverted) {
        const int width=local_crop[2]*2,height=local_crop[3]*2;
        // At most188x48 or168x80, doubled: less than216KiB BGRA.
        std::vector<std::uint8_t> memory(static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*4u);
        const int roi_height=static_cast<int>(ink.size()/static_cast<std::size_t>(roi_width));
        for(int y=0;y<height;++y)for(int x=0;x<width;++x){
            const int sx=local_crop[0]+x/2,sy=local_crop[1]+y/2;
            const auto value=sx>=0&&sx<roi_width&&sy>=0&&sy<roi_height?
                ink[static_cast<std::size_t>(sy)*static_cast<std::size_t>(roi_width)+static_cast<std::size_t>(sx)]:std::uint8_t{0};
            const auto c=inverted?static_cast<std::uint8_t>(255-value):value;
            const auto offset=(static_cast<std::size_t>(y)*static_cast<std::size_t>(width)+static_cast<std::size_t>(x))*4u;
            memory[offset]=memory[offset+1]=memory[offset+2]=c;memory[offset+3]=255;
        }
        winrt::Windows::Storage::Streams::DataWriter writer;
        writer.WriteBytes(winrt::array_view<const std::uint8_t>(memory));
        return Bitmap::CreateCopyFromBuffer(writer.DetachBuffer(),
            winrt::Windows::Graphics::Imaging::BitmapPixelFormat::Bgra8,width,height,
            winrt::Windows::Graphics::Imaging::BitmapAlphaMode::Premultiplied);
    }
    NumericHudRoiResult recognize(Engine& engine,std::span<const std::uint8_t> ink,int width,int height,
            int source_x,int source_y,bool score) noexcept {
        NumericHudRoiResult result;
        try{
            int x0=width,y0=height,x1=-1,y1=-1;
            for(int y=0;y<height;++y)for(int x=0;x<width;++x){
                if(!ink[static_cast<std::size_t>(y)*static_cast<std::size_t>(width)+static_cast<std::size_t>(x)])continue;
                x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x);y1=std::max(y1,y);
            }
            if(x1<0){result.ocr_status=NumericHudOcrStatus::no_ink;return result;}
            const std::array<int,4> crop{x0-4,y0-4,x1-x0+9,y1-y0+9};
            result.crop={source_x+crop[0],source_y+crop[1],crop[2],crop[3]};
            for(unsigned attempt=0;attempt<2;++attempt){
                if(stopping_.load(std::memory_order_acquire)){result.ocr_status=NumericHudOcrStatus::cancelled;return result;}
                result.inverted=attempt!=0;++result.attempts;
                auto pixels=bitmap(ink,width,crop,result.inverted);
                const auto start=std::chrono::steady_clock::now();
                auto operation=engine.RecognizeAsync(pixels);
                const auto status=operation.wait_for(std::chrono::milliseconds(100));
                result.ocr_elapsed_us+=elapsed_us(start);
                if(status==winrt::Windows::Foundation::AsyncStatus::Started){
                    operation.Cancel();result.ocr_status=NumericHudOcrStatus::timeout;
                    ++summary_.ocr_timeouts;summary_.status="ocr_timeout";stopping_.store(true,std::memory_order_release);return result;
                }
                if(stopping_.load(std::memory_order_acquire)){
                    result.ocr_status=NumericHudOcrStatus::cancelled;return result;
                }
                if(status==winrt::Windows::Foundation::AsyncStatus::Canceled){result.ocr_status=NumericHudOcrStatus::cancelled;return result;}
                auto recognized=operation.GetResults();
                const auto text=recognized.Text();result.raw.assign(std::wstring_view{text.c_str(),text.size()});
                const auto parsed=score?parse_numeric_hud_score(result.raw):parse_numeric_hud_combo(result.raw,maximum_combo_);
                result.value=parsed.value;result.parse_status=parsed.status;result.ocr_status=NumericHudOcrStatus::recognized;
                if(result.value)break;
            }
        }catch(const winrt::hresult_error& error){
            result.ocr_status=NumericHudOcrStatus::error;++summary_.ocr_errors;summary_.status="ocr_error";
            const auto text=error.message();summary_.error.assign(std::wstring_view{text.c_str(),text.size()});
            stopping_.store(true,std::memory_order_release);
        }catch(...){
            result.ocr_status=NumericHudOcrStatus::error;++summary_.ocr_errors;summary_.status="ocr_error";
            summary_.error.assign("bounded OCR diagnostic failed");stopping_.store(true,std::memory_order_release);
        }
        return result;
    }
    void run() noexcept {
        bool apartment=false;
        try{
            if(!SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL))
                throw std::runtime_error("Cannot set numeric OCR worker below-normal priority");
            winrt::init_apartment(winrt::apartment_type::multi_threaded);apartment=true;
            auto engine=Engine::TryCreateFromLanguage(winrt::Windows::Globalization::Language(L"en-US"));
            if(!engine){summary_.status="engine_missing";summary_.error.assign("Installed en-US built-in OCR unavailable");}
            else{
                summary_.engine_language=winrt::to_string(engine.RecognizerLanguage().LanguageTag());
                summary_.status="available";summary_.available=true;
                if(!stopping_.load(std::memory_order_acquire)){
                    slot_.store(Slot::idle,std::memory_order_release);available_.store(true,std::memory_order_release);
                }
                while(!stopping_.load(std::memory_order_acquire)){
                    if(slot_.load(std::memory_order_acquire)!=Slot::queued){
                        std::unique_lock lock(wake_mutex_);
                        wake_.wait_for(lock,std::chrono::milliseconds(5),[this]{return stopping_.load(std::memory_order_acquire)||
                            slot_.load(std::memory_order_acquire)==Slot::queued;});
                        continue;
                    }
                    slot_.store(Slot::processing,std::memory_order_release);
                    NumericHudRow row;row.sample_index=rows_.size();row.frame=frame_;row.progress_bar=progress_bar_;
                    const auto begin=std::chrono::steady_clock::now();
                    row.score=recognize(engine,score_pixels_,180,40,1076,20,true);
                    row.combo=recognize(engine,combo_pixels_,160,72,560,8,false);
                    row.processing_elapsed_us=elapsed_us(begin);
                    if(row.score.value)++summary_.score_values;if(row.combo.value)++summary_.combo_values;
                    if(row.score.ocr_status==NumericHudOcrStatus::cancelled||row.combo.ocr_status==NumericHudOcrStatus::cancelled)
                        ++summary_.cancelled_rows;
                    // All7200 row slots were reserved before worker startup.
                    if(rows_.size()<numeric_hud_max_rows){rows_.push_back(std::move(row));++summary_.completed;}
                    clear_pixels();
                    if(!stopping_.load(std::memory_order_acquire))slot_.store(Slot::idle,std::memory_order_release);
                }
            }
        }catch(const winrt::hresult_error& error){
            summary_.status="worker_error";const auto text=error.message();
            summary_.error.assign(std::wstring_view{text.c_str(),text.size()});++summary_.ocr_errors;
        }catch(...){summary_.status="worker_error";summary_.error.assign("Numeric OCR worker startup/runtime failed");++summary_.ocr_errors;}
        // request_stop may come from another thread while the producer owns a
        // copying slot. Do not touch that slot's bytes on worker exit. The
        // producer discards it or finish clears it after producer/worker exit.
        available_.store(false,std::memory_order_release);slot_.store(Slot::stopped,std::memory_order_release);
        if(apartment)winrt::uninit_apartment();
    }
#endif
public:
    explicit NumericHudObserver(std::size_t maximum_combo) noexcept:maximum_combo_(maximum_combo) {
        summary_.maximum_combo=maximum_combo;
        try{
            if(!maximum_combo||maximum_combo>200'000){summary_.status="invalid_limit";slot_.store(Slot::stopped);return;}
#if RHYTHM_EXPERIMENTAL_NUMERIC_HUD_WINRT
            rows_.reserve(numeric_hud_max_rows);summary_.status="starting";
            worker_=std::thread([this]{run();});
#else
            summary_.status="unsupported";summary_.error.assign("Windows built-in OCR headers/runtime unavailable");
            slot_.store(Slot::stopped);
#endif
        }catch(...){summary_.status="start_failed";summary_.error.assign("Numeric OCR allocation/thread startup failed");slot_.store(Slot::stopped);}
    }
    NumericHudObserver(const NumericHudObserver&)=delete;
    NumericHudObserver& operator=(const NumericHudObserver&)=delete;
    ~NumericHudObserver(){finish();}
    bool available() const noexcept{return available_.load(std::memory_order_acquire);}
    void submit(std::uint64_t sequence,std::optional<std::uint64_t> source_sequence,
            std::optional<std::int64_t> source_timestamp_us,std::int64_t received_ns,std::int64_t processed_ns,
            std::uint64_t epoch,std::uint64_t generation,std::uint64_t geometry_version,
            std::uint32_t width,std::uint32_t height,std::uint32_t stride,std::span<const std::uint8_t> rgb) noexcept {
        NumericHudFrame next;next.sequence=sequence;next.source_sequence=source_sequence;next.source_timestamp_us=source_timestamp_us;
        next.received_ns=received_ns;next.processed_ns=processed_ns;next.epoch=epoch;next.generation=generation;next.geometry_version=geometry_version;
        auto expected=Slot::idle;
        const bool claimed=slot_.compare_exchange_strong(expected,Slot::copying,std::memory_order_acq_rel);
        const auto outcome=budget_.observe(next,width,height,stride,rgb.size(),claimed,
            available_.load(std::memory_order_acquire)&&!stopping_.load(std::memory_order_acquire));
        if(outcome!=NumericHudSubmitOutcome::submitted){
            if(claimed)slot_.store(Slot::idle,std::memory_order_release);return;
        }
        frame_=next;
        // Same actual frame/receipt as score/combo; no extra capture or pixels
        // retained. Classification never affects permissions or deadlines.
        progress_bar_=measure_progress_bar_metadata(static_cast<int>(width),static_cast<int>(height),static_cast<int>(stride),rgb);
        copy_ink(rgb,stride,1076,20,180,40,score_pixels_.data());
        copy_ink(rgb,stride,560,8,160,72,combo_pixels_.data());
        if(stopping_.load(std::memory_order_acquire)){
            clear_pixels();slot_.store(Slot::stopped,std::memory_order_release);return;
        }
        expected=Slot::copying;
        if(!slot_.compare_exchange_strong(expected,Slot::queued,std::memory_order_release,std::memory_order_relaxed)){
            clear_pixels();return;
        }
        wake_.notify_one();
    }
    void submit(std::uint64_t sequence,std::uint64_t source_sequence,std::int64_t source_timestamp_us,
            std::int64_t received_ns,std::int64_t processed_ns,std::uint64_t epoch,std::uint64_t generation,
            std::uint64_t geometry_version,std::uint32_t width,std::uint32_t height,std::uint32_t stride,
            std::span<const std::uint8_t> rgb) noexcept {
        submit(sequence,std::optional<std::uint64_t>{source_sequence},source_timestamp_us<0?std::nullopt:
            std::optional<std::int64_t>{source_timestamp_us},received_ns,processed_ns,epoch,generation,geometry_version,width,height,stride,rgb);
    }
    void request_stop() noexcept {stopping_.store(true,std::memory_order_release);wake_.notify_one();}
    // Call only from the producer, outside submit and after release-all. A
    // request_stop alone may safely come from a different thread. The worker checks stop between every ROI and
    // polarity, and waits at most100ms for an individual asynchronous OCR call.
    // OS scheduling/COM startup/cancellation are not hard real-time guarantees.
    void finish() noexcept {
        request_stop();
        if(worker_.joinable())worker_.join();
        summary_.submission=budget_.stats();summary_.finished=true;
        if(summary_.status=="available"||summary_.status=="starting")summary_.status="finished";
        clear_pixels();
    }
    // Single-owner access only AFTER finish, when the worker has joined.
    const std::vector<NumericHudRow>& rows() const noexcept{return rows_;}
    const NumericHudObserverSummary& summary() const noexcept{return summary_;}
};
} // namespace rhythm::experimental
