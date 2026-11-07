#pragma once

#include "rhythm/contact_compiler.hpp"
#include <charconv>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace rhythm::experimental {
// A static, explicitly requested experiment. This never updates a live clock,
// changes source song timestamps, or supplies synchronization permission.
struct ClockProbe {
    bool enabled=false;
    SongNs cut_ns=0,delta_ns=0;
    SongNs offset_at(SongNs song_ns) const;
    SongNs mapped_song_ns(SongNs song_ns) const;
};

inline void validate_clock_probe_options(const ClockProbe& probe) {
    if(!probe.enabled) {
        if(probe.cut_ns!=0||probe.delta_ns!=0)
            throw std::invalid_argument("disabled clock probe must have zero cut and delta");
        return;
    }
    if(probe.cut_ns<0||probe.cut_ns>600'000'000'000||
       probe.delta_ns< -250'000'000||probe.delta_ns>250'000'000)
        throw std::invalid_argument("clock probe cut must be0..600s and delta within250ms");
}
inline SongNs ClockProbe::offset_at(SongNs song_ns) const {
    return enabled&&song_ns>=cut_ns?delta_ns:0;
}
inline SongNs ClockProbe::mapped_song_ns(SongNs song_ns) const {
    validate_clock_probe_options(*this);
    if(song_ns<0)throw std::invalid_argument("clock probe cannot map a negative song timestamp");
    const auto delta=offset_at(song_ns);
    if(delta>0&&song_ns>std::numeric_limits<SongNs>::max()-delta)
        throw std::overflow_error("clock probe song mapping overflows");
    if(delta<0&&song_ns< -delta)
        throw std::invalid_argument("clock probe song mapping would become negative");
    return song_ns+delta;
}
inline ClockProbe parse_clock_probe_ms(std::string_view text) {
    const auto separator=text.find(':');
    if(separator==std::string_view::npos||separator==0||separator+1==text.size()||
       text.find(':',separator+1)!=std::string_view::npos||text.size()>32)
        throw std::invalid_argument("clock probe requires integer cut_ms:delta_ms");
    const auto integer=[](std::string_view value) {
        SongNs number=0;
        const auto parsed=std::from_chars(value.data(),value.data()+value.size(),number);
        if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size())
            throw std::invalid_argument("clock probe requires strict decimal integers");
        return number;
    };
    const auto cut=integer(text.substr(0,separator)),delta=integer(text.substr(separator+1));
    if(cut<0||cut>600'000||delta< -250||delta>250)
        throw std::invalid_argument("clock probe cut_ms must be0..600000 and delta_ms within250");
    ClockProbe result{true,cut*1'000'000,delta*1'000'000};validate_clock_probe_options(result);return result;
}

struct ClockProbeProof {
    bool enabled=false;
    SongNs cut_ns=0,delta_ns=0;
    std::size_t prefix_events=0,suffix_events=0,prefix_contacts=0,suffix_contacts=0;
    SongNs original_first_ns=0,original_last_ns=0,mapped_first_ns=0,mapped_last_ns=0;
    std::string source_cfp,map_identity;
};

inline ClockProbeProof validate_clock_probe(const CompiledContactPlan& plan,const ClockProbe& probe) {
    validate_clock_probe_options(probe);
    if(plan.events.empty()||plan.memory.contacts.empty())
        throw std::invalid_argument("clock probe requires a nonempty original compiled plan");
    ClockProbeProof proof;proof.enabled=probe.enabled;proof.cut_ns=probe.cut_ns;proof.delta_ns=probe.delta_ns;
    proof.source_cfp=plan.metadata.cache_sha256;
    proof.map_identity=probe.enabled?"experimental-static-suffix-offset-v1;source_cfp="+proof.source_cfp+
        ";cut_ns="+std::to_string(probe.cut_ns)+";delta_ns="+std::to_string(probe.delta_ns):"disabled-identity";
    std::size_t contact_steps=0;
    for(const auto& contact:plan.memory.contacts) {
        if(contact.steps.empty())throw std::invalid_argument("clock probe contact is empty");
        const auto first=contact.steps.front().song_ns,last=contact.steps.back().song_ns;
        if(first<0||last<first)throw std::invalid_argument("clock probe contact bounds are invalid");
        if(probe.enabled&&first<probe.cut_ns&&last>=probe.cut_ns)
            throw std::invalid_argument("clock probe cutoff straddles a contact");
        const bool suffix=probe.enabled&&first>=probe.cut_ns;
        if(suffix)++proof.suffix_contacts;else ++proof.prefix_contacts;
        SongNs previous=-1;
        for(const auto& step:contact.steps) {
            if(step.song_ns<=previous||probe.offset_at(step.song_ns)!=probe.offset_at(first))
                throw std::invalid_argument("clock probe would change contact internal timing");
            (void)probe.mapped_song_ns(step.song_ns);previous=step.song_ns;
            if(contact_steps==std::numeric_limits<std::size_t>::max())throw std::overflow_error("clock probe step count overflows");
            ++contact_steps;
        }
    }
    if(contact_steps!=plan.events.size()||plan.total_steps!=plan.events.size())
        throw std::invalid_argument("clock probe original plan event count differs");
    SongNs previous_song=-1,previous_mapped=-1;
    Phase previous_phase=Phase::up;std::uint64_t previous_id=0;
    for(const auto& event:plan.events) {
        const auto mapped=probe.mapped_song_ns(event.song_ns);
        if(event.song_ns<previous_song||mapped<previous_mapped||
           (event.song_ns>previous_song&&mapped<=previous_mapped)||
           (event.song_ns==previous_song&&(mapped!=previous_mapped||phase_order(event.phase)<phase_order(previous_phase)||
               (event.phase==previous_phase&&event.contact_id<=previous_id))))
            throw std::invalid_argument("clock probe does not preserve every original event order and tie");
        if(probe.enabled&&event.song_ns>=probe.cut_ns)++proof.suffix_events;else ++proof.prefix_events;
        previous_song=event.song_ns;previous_mapped=mapped;previous_phase=event.phase;previous_id=event.contact_id;
    }
    proof.original_first_ns=plan.events.front().song_ns;proof.original_last_ns=plan.events.back().song_ns;
    proof.mapped_first_ns=probe.mapped_song_ns(proof.original_first_ns);proof.mapped_last_ns=probe.mapped_song_ns(proof.original_last_ns);
    return proof;
}
} // namespace rhythm::experimental
