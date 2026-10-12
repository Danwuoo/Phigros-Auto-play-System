# Source-only performance deltas. All old numerical gates stay binding.
function ComponentDeltaPerformanceBuildGroups([bool]$dense,[bool]$frame){
 ComponentDeltaColdProducerRequire ($dense -or $frame) 'explicit performance build groups'
 $first=@('game_motion_delta');if($dense){$first+=@('game_delta')}
 $first+=@('candidate_admission_delta','worker_affinity_controls','qualification_reader','qualification_reader_controls')
 if($frame){$first+=@('frame_metadata_controls')}
 return @(@{targets=$first},@{targets=@('current_chain','production_pipeline_cold','score_only_controls','pas','candidate_admission_controls','motion_source_regression')})
}
function ComponentDeltaDenseTargets {return @('current_chain','production_pipeline_cold','score_only_controls','pas','candidate_admission_controls','motion_source_regression')}
function ComponentDeltaDenseCases {return @(
 'GameObserver.MultiplePixelsTracksAndRelativeMovingLinePrediction',
 'GameObserver.CrossingCurrentVerticalRidgeSurvivesConnectedHorizontalLine',
 'GameObserver.CurrentVerticalSearchRejectsHoldSideRailsAndBroadWhiteFill',
 'GameObserver.CrossingVerticalWhiteRidgeSurvivesCurrentWarmHitTintButCannotBeBornFromTint',
 'GameObserver.SplitsPixelConnectedCrossingRidgesWithoutInventingHoldRails',
 'GameObserver.UiLossCancelsAndCapacityIsBounded',
 'GameObserver.OnePixelLineJoinedToHoldBorderStaysObservable',
 'GameObserver.NestedRibbonsMergeButEqualWidthOverlapsRemainDistinct',
 'GameObserver.YellowOutlineDoesNotDuplicateBlueCoreAndHoldHasLeadingEdgeAndTail',
 'GameObserver.SmallHitParticlesAndYellowRingsCannotBecomeNoteTargets',
 'GameObserver.InterruptedThinBandIsOneLineButSeparatedBandsRemainDistinct',
 'GameObserver.PerpendicularColoredRibbonsKeepTheirOwnGeometryAndKind')}
function ComponentDeltaDenseFilter {return (ComponentDeltaDenseCases) -join ':'}
function ComponentDeltaDenseSourceText($basePin,$currentPin){
 CheckEntry $basePin;CheckEntry $currentPin
 $base=[Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($basePin.path))
 $current=[Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes($currentPin.path))
 $old=@'
    // Finish the current line set before interpreting note ribbons. Source
    // component order changes when a rotating line is interrupted by a note.
    // A note's local dimensions must not depend on which fragment came first.
    for(const auto& c:all) {
        const double w=c.x1-c.x0+2.0,h=c.y1-c.y0+2.0;
'@
 $new=@'
    // Finish the current line set before interpreting note ribbons. Source
    // component order changes when a rotating line is interrupted by a note.
    // A note's local dimensions must not depend on which fragment came first.
    for(const auto& c:all) {
        if(c.color==4) continue;
        const double w=c.x1-c.x0+2.0,h=c.y1-c.y0+2.0;
'@
 ComponentDeltaColdProducerRequire ([regex]::Matches($base,[regex]::Escape($old)).Count -eq 1 -and [regex]::Matches($current,[regex]::Escape($new)).Count -eq 1) 'one exact Dense old/new note loop'
 ComponentDeltaColdProducerRequire ($current -ceq $base.Replace($old,$new) -and $current.Replace($new,$old) -ceq $base) 'Dense sole insertion; BOM/EOL and full inverse preserved'
 ComponentDeltaColdProducerRequire ([Text.Encoding]::UTF8.GetByteCount($base) -eq $basePin.bytes -and [Text.Encoding]::UTF8.GetByteCount($current) -eq $currentPin.bytes) 'Dense exact source bytes'
}
function ComponentDeltaDenseGTest($report){
 $expected=@(ComponentDeltaDenseCases)
 foreach($field in @('failures','errors','disabled')){$null=ComponentDeltaColdProducerInteger $report $field 0 0}
 $null=ComponentDeltaColdProducerInteger $report 'tests' $expected.Count $expected.Count
 $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
 foreach($suite in $report.testsuites){foreach($case in $suite.testsuite){
  ComponentDeltaColdProducerText $case 'status' 'RUN';ComponentDeltaColdProducerText $case 'result' 'COMPLETED'
  ComponentDeltaColdProducerRequire (-not($case.PSObject.Properties.Name -ccontains 'failures') -or @($case.failures).Count -eq 0) 'Dense no failure records'
  ComponentDeltaColdProducerRequire ($names.Add($suite.name+'.'+$case.name)) 'unique Dense actual case'
 }}
 ComponentDeltaColdProducerRequire ($names.Count -eq $expected.Count -and @(Compare-Object (@($names)|Sort-Object) ($expected|Sort-Object)).Count -eq 0) 'exact separate Dense original cases'
}
function ComponentDeltaFrameSourceText($basePin,$currentPin){
 CheckEntry $basePin;CheckEntry $currentPin
 $base=[IO.File]::ReadAllText($basePin.path);$current=[IO.File]::ReadAllText($currentPin.path)
 $old='Frame f;f.sequence=i;f.epoch=f.generation=f.geometry_version=1;f.width=1280;f.height=720;f.stride=3840;f.source_rotation=1;f.capture_backend="offline_memory_stub";f.capture_complete_ns=f.pixels_ready_ns=sample.render_end;'
 $fresh='const auto& f=producer_metadata.for_attempt(static_cast<std::uint64_t>(i),sample.render_end);'
 $anchor='#include "producer_wait.hpp"';$begin=' const auto begin=clock.now_ns();'
 foreach($text in @($old,$anchor,$begin)){ComponentDeltaColdProducerRequire ([regex]::Matches($base,[regex]::Escape($text)).Count -eq 1) 'one original Frame literal site'}
 $expected=$base.Replace($anchor,$anchor+"`n"+'#include "frame_metadata.hpp"').Replace($begin,' cold::frame_metadata::FixedMetadata producer_metadata;'+"`n"+$begin).Replace($old,$fresh)
 ComponentDeltaColdProducerRequire ($current -ceq $expected) 'only fixed metadata include/setup/getter; all clock, pixel, epoch and policy statements unchanged'
}
