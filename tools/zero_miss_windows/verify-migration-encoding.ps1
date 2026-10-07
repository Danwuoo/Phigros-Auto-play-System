param([Parameter(Mandatory)][string]$SourceMigration,
      [Parameter(Mandatory)][string]$FreshRoot)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $FreshRoot){throw 'fresh-encoding-root-required'}
[IO.Directory]::CreateDirectory($FreshRoot)|Out-Null
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile(
 (Join-Path $PSScriptRoot '../../research/prelive_current/run.ps1'),[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'actual-wrapper-parser-errors'}
# Execute only these exact file-only functions from the actual reviewed wrapper.
foreach($name in @('CheckEntry','ByteVariants')){
 $function=$ast.FindAll({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst]},$false)|Where-Object Name -ceq $name
 if(@($function).Count -ne 1){throw 'actual-wrapper-function-not-unique'}
 . ([scriptblock]::Create($function.Extent.Text))
}
$packet=[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($SourceMigration))
$migration=Get-Content -LiteralPath $SourceMigration -Raw|ConvertFrom-Json
$cases=@()
foreach($row in $migration.paths){
 $binding=$row.canonical_target|Select-Object path,bytes,sha256
 $binding.path=Join-Path $packet $binding.path
 $actual=@(ByteVariants $binding)
 foreach($expected in $row.target_encodings){
  $match=@($actual|Where-Object {$_.encoding -ceq $expected.encoding -and $_.bytes -eq $expected.bytes -and $_.sha256 -ceq $expected.sha256})
  $cases+=@(@{id=$row.relative_path+':'+$expected.encoding;pass=($match.Count -eq 1)})
 }
}
$utf8=[Text.UTF8Encoding]::new($false,$true)
$negative=@(@{id='BOM';bytes=$utf8.GetBytes(([string][char]0xfeff)+"x`n")},
 @{id='CRLF';bytes=$utf8.GetBytes("x`r`n")},
 @{id='invalid-UTF8';bytes=[byte[]]@(0xc3,0x28)})
foreach($case in $negative){
 $path=Join-Path $FreshRoot ($case.id+'.bin');[IO.File]::WriteAllBytes($path,$case.bytes)
 $binding=@{path=$path;bytes=$case.bytes.Length;sha256=(Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant()}
 $rejected=$false;try{ByteVariants $binding|Out-Null}catch{$rejected=$true}
 $cases+=@(@{id=$case.id;pass=$rejected})
}
$report=@{schema='pas.windows-migration-encoding-controls.v1';cases=$cases;failed=@($cases|Where-Object {-not $_.pass}).Count;actual_CreateProcess=0;device_operations=0;source=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot '../../research/prelive_current/run.ps1')).Hash.ToLowerInvariant()}
[IO.File]::WriteAllText((Join-Path $FreshRoot 'report.json'),($report|ConvertTo-Json -Depth 8),$utf8)
$report|Select-Object failed,actual_CreateProcess,device_operations,@{n='checks';e={$_.cases.Count}}|ConvertTo-Json
if($report.failed){exit 1}
