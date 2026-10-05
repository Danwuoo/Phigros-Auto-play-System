$ErrorActionPreference='Stop'
$repoPath=(Resolve-Path "$PSScriptRoot/..").Path
$batchPath="$repoPath/measurements/game-assist/2026-09-30-m0-manual-continue/runtime-x11-p"
function Hash($p){(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$paths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$projects=@()
foreach($role in @('B0','B1')){
 $build="$repoPath/out/x11-p/build-$role"
 foreach($name in @('pas','pas_core','pas_emulator','pas_proto','pas_bench','x11_cost')){
  $p="$build/$name.vcxproj";[xml]$xml=Get-Content $p -Raw
  $ns=[Xml.XmlNamespaceManager]::new($xml.NameTable);$ns.AddNamespace('m','http://schemas.microsoft.com/developer/msbuild/2003')
  $groups=@($xml.SelectNodes('//m:ItemDefinitionGroup',$ns)|Where-Object Condition -Match 'Release\|x64')
  $projects+=@{role=$role;target=$name;project_sha256=(Hash $p);release_settings=@($groups|ForEach-Object OuterXml)}
  foreach($g in $groups){if($g.Link.AdditionalDependencies){foreach($d in $g.Link.AdditionalDependencies.Split(';')){if([IO.Path]::IsPathRooted($d) -and (Test-Path -LiteralPath $d -PathType Leaf)){[void]$paths.Add($d)}}}}
 }
 foreach($f in Get-ChildItem "$build" -Filter '*.read.1.tlog' -File -Recurse){
  foreach($line in [IO.File]::ReadAllLines($f.FullName)){
   if($line -match '^[A-Z]:\\' -and (Test-Path -LiteralPath $line -PathType Leaf)){[void]$paths.Add($line)}
  }
 }
}
$compilerRoot='C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231'
foreach($p in @("$compilerRoot/bin/Hostx64/x64/cl.exe","$compilerRoot/bin/Hostx64/x64/link.exe","$compilerRoot/bin/Hostx64/x64/dumpbin.exe",(Get-Command cmake).Source,(Get-Command MSBuild -ErrorAction SilentlyContinue).Source,"$repoPath/out/vcpkg_installed/x64-windows/tools/protobuf/protoc.exe","$repoPath/out/vcpkg_installed/x64-windows/tools/grpc/grpc_cpp_plugin.exe")){
 if($p -and (Test-Path -LiteralPath $p)){[void]$paths.Add($p)}
}
$files=@();foreach($p in $paths|Sort-Object){$f=Get-Item -LiteralPath $p;$files+=@{path=$f.FullName;sha256=(Hash $f.FullName);bytes=$f.Length}}
$output="$batchPath/compiled-dependency-freeze.json";if(Test-Path $output){throw 'existing'}
[IO.File]::WriteAllText($output,(@{files=$files;projects=$projects;scope='actual CL/link read tlogs plus absolute imported libraries/compiler/proto tools; references only, no dependency reinstall/copy';dependency_runtime='runtime-closure.json';system_dlls='OS 10.0.26200; API sets resolved by OS; no portable OS freeze'}|ConvertTo-Json -Depth 60)+"`n",[Text.UTF8Encoding]::new($false))
Write-Output "dependency inputs=$($files.Count)"
