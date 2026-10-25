#pragma once
#include "rhythm/chart_memory.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace rhythm::experimental {
// This is an offline extraction attestation bound to pinned local APKs by the
// panel. It identifies native FSB5 bytes, never a fabricated decoded WAV/PCM.
inline std::string installed_catalog_audio_identity(const nlohmann::json& source) {
    const auto track=source.at("track_id").get<std::string>();
    const auto difficulty=source.at("difficulty").get<std::string>();
    const auto& audio=source.at("audio_identity");
    const auto hash=audio.at("sha256").get<std::string>();
    if(source.at("schema")!="pas.installed-catalog-song-source.v1"||
       source.at("package")!="com.PigeonGames.Phigros"||source.at("version_name")!="4.0.1"||
       source.at("version_code")!=157||source.at("exact_chart_bytes_verified")!=true||
       (difficulty!="IN"&&difficulty!="AT")||track.empty()||track.size()>256||
       !sha256_text(hash)||!sha256_text(source.at("chart").at("sha256").get<std::string>())||
       audio.at("format")!="native-fsb5"||audio.at("catalog_key")!="Assets/Tracks/"+track+"/music.wav"||
       source.at("chart").at("source").at("catalog_key")!="Assets/Tracks/"+track+"/Chart_"+difficulty+".json"||
       !source.at("packages").is_array()||source.at("packages").empty()||source.at("packages").size()>16)
        throw std::invalid_argument("installed catalog chart/audio/difficulty identity mismatch");
    for(const auto& package:source.at("packages"))
        if(!sha256_text(package.at("sha256").get<std::string>()))
            throw std::invalid_argument("installed catalog APK hash missing");
    return hash;
}
inline bool valid_adaptive_hud_policy(const std::string& policy) {
    return policy=="calibrated-title-v1"||policy=="shared-in-prefix-score-v1"||
        policy=="shared-in-cell-score-v2"||policy=="shared-in-subpixel-prefix-score-v3"||
        policy=="shared-difficulty-subpixel-prefix-score-v4";
}
inline bool hud_policy_matches_difficulty(const std::string& policy,const std::string& difficulty) {
    return policy=="calibrated-title-v1"||difficulty=="IN"||
        (policy=="shared-difficulty-subpixel-prefix-score-v4"&&difficulty=="AT");
}
} // namespace rhythm::experimental
