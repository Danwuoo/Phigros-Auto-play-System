#pragma once
#include <nlohmann/json.hpp>
#include <utility>
namespace pas::diagnostic_json {
template<class T> std::pair<const char*,nlohmann::json> field(const char* key,T&& value){
 return {key,nlohmann::json(std::forward<T>(value))};
}
template<class... Fields> nlohmann::json object(Fields&&... fields){
 nlohmann::json::object_t result;
 (result.emplace(fields.first,std::move(fields.second)),...);
 return nlohmann::json(std::move(result));
}
}
