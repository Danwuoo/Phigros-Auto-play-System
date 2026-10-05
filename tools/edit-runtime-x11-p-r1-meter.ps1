$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$p="$repoPath/apps/runtime_x11_p_r1/main.cpp";$s=[IO.File]::ReadAllText($p).Replace("`r`n","`n")
$s="#include `"pas/action_wake.hpp`"`n#include `"pas/session_archive.hpp`"`n#include `"pas/session_events.hpp`"`n#include `"pas/game_session.hpp`"`n#include `"meter_touch.hpp`"`n#include `"archive_verify.hpp`"`n"+$s
$s=$s.Replace('Nanoseconds start) {','Nanoseconds start,std::uint64_t limit=128) {').Replace('k<=128','k<=limit')
$b=$s.IndexOf('class MeterTouch final:');$e=$s.IndexOf('json dist(',$b);$s=$s.Remove($b,$e-$b).Insert($b,"using r1::MeterTouch;`n")
$b=$s.IndexOf('// Harness-only writer load:');$e=$s.IndexOf('void save(',$b);$s=$s.Remove($b,$e-$b)
$b=$s.IndexOf('json run(');$e=$s.IndexOf('// Public-observable bridge:',$b)
$run=[IO.File]::ReadAllText("$repoPath/apps/runtime_x11_p_r1/run.inc").Replace("`r`n","`n")
$s=$s.Remove($b,$e-$b).Insert($b,$run+"`n")
# Streaming bridge comparison: same output bytes, bounded single row, no duplicate 30MB retention.
$b=$s.IndexOf('void bridge(');$s=$s.Insert($b,@'
class CompareBuffer final:public std::streambuf {
    std::ifstream reference_;std::string row_;std::uint64_t rows_=0,bytes_=0;bool equal_=true;
protected:
    int overflow(int c) override {if(c==traits_type::eof())return traits_type::not_eof(c);char ch=static_cast<char>(c);row_+=ch;
        if(row_.size()>256*1024)throw std::runtime_error("bridge row capacity");
        if(ch=='\n'){std::string old;if(!std::getline(reference_,old)||old+"\n"!=row_)equal_=false;++rows_;bytes_+=row_.size();row_.clear();}return c;}
public:
    explicit CompareBuffer(const std::filesystem::path& p):reference_(p){if(!reference_)throw std::runtime_error("bridge reference missing");}
    json finish(){std::string extra;equal_=equal_&&row_.empty()&&!std::getline(reference_,extra)&&reference_.eof();return {{"equal",equal_},{"rows",rows_},{"bytes",bytes_}};}
};
'@+"`n")
$s=$s.Replace('void bridge(const std::filesystem::path& out) {','void bridge(const std::filesystem::path& out,const std::filesystem::path& reference) {')
$s=$s.Replace('std::ofstream f(out/"public-events.jsonl");','CompareBuffer buffer(reference);std::ostream f(&buffer);')
$s=$s.Replace('f.close();save(out/"summary.json",','auto comparison=buffer.finish();if(!comparison.at("equal").get<bool>())throw std::runtime_error("bridge bytes differ");save(out/"summary.json",')
$s=$s.Replace('{"events_sha256",sha256_file(out/"public-events.jsonl")}', '{"comparison",comparison},{"reference_sha256",sha256_file(reference)}')
$s=$s.Replace('if(argc==3&&std::string(argv[1])=="bridge"){bridge(argv[2]);return 0;}', 'if(argc==4&&std::string(argv[1])=="bridge"){bridge(argv[2],argv[3]);return 0;}')
$s=$s.Replace('return r.at("hard_gate").get<bool>()?0:2;', 'return r.at("hard_gate").get<bool>()?0:2;')
[IO.File]::WriteAllText($p,$s,[Text.UTF8Encoding]::new($false))
$p="$repoPath/apps/runtime_x11_p_r1/gate.cpp";$s=[IO.File]::ReadAllText($p).Replace("`r`n","`n")
$b=$s.IndexOf('bool normal(');$e=$s.IndexOf('int main(',$b)
$s=$s.Remove($b,$e-$b).Insert($b,"using r1::normal;`n");$s="#include `"gate_contract.hpp`"`n"+$s
$s=$s.Replace('pass=pass&&normal(runs[i]);','pass=normal(runs[i])&&pass;result["normal_hard_gates"][load][std::to_string(i+1)]=normal(runs[i]);')
$s=$s.Replace('if(std::string(argv[1])=="noise"){','if(std::string(argv[1])=="check"){auto r=read(root/"summary.json");std::cout<<json{{"normal_gate",normal(r)},{"action_timing",r.value("action_expectation","")=="none"?"not-applicable":"required"}}.dump();return normal(r)?0:2;}\n if(std::string(argv[1])=="noise"){'.Replace('\n',"`n"))
$s=$s.Replace('f<<j.dump(2)<<''\n'';}', 'if(!f)throw std::runtime_error("gate output open");f<<j.dump(2)<<''\n'';f.flush();if(!f)throw std::runtime_error("gate output write");}')
[IO.File]::WriteAllText($p,$s,[Text.UTF8Encoding]::new($false))
