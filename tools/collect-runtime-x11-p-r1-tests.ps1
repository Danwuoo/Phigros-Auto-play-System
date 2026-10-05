function Get-R1TestResult([string]$Path) {
 [xml]$x=Get-Content -LiteralPath $Path -Raw
 $cases=@($x.SelectNodes('//testcase'))
 if(!$cases.Count){throw 'test XML has no testcase rows'}
 $fail=@($cases|Where-Object {$_.SelectNodes('failure').Count -gt 0}|ForEach-Object {"$($_.classname).$($_.name)"})
 $skip=@($cases|Where-Object {$_.SelectNodes('skipped').Count -gt 0 -or $_.result -eq 'skipped'})
 $errors=@($cases|Where-Object {$_.SelectNodes('error').Count -gt 0})
 $disabled=@($cases|Where-Object {$_.status -eq 'notrun' -and $_.result -ne 'skipped'})
 [pscustomobject]@{tests=$cases.Count;passed=$cases.Count-$fail.Count-$skip.Count-$errors.Count-$disabled.Count;failures=$fail.Count;skipped=$skip.Count;errors=$errors.Count;disabled=$disabled.Count;failed_names=$fail;skipped_names=@($skip|ForEach-Object {"$($_.classname).$($_.name)"});root_skipped=$x.testsuites.skipped;sha256=(Get-FileHash $Path -Algorithm SHA256).Hash.ToLowerInvariant()}
}
