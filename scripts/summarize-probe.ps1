param([string]$Csv='results/audio-probe.csv',[string]$Output='results/audio-probe-statistics.json')
$ErrorActionPreference='Stop'
$rows=@(Import-Csv -LiteralPath $Csv)
if($rows.Count -lt 2) { throw 'Probe log has fewer than two samples' }
if(-not ($rows[0].PSObject.Properties.Name -contains 'callback_count')) { throw 'This is an older log without callback watchdog data' }
$stalls=0
for($i=1;$i -lt $rows.Count;$i++) {
    if([long]$rows[$i].callback_count -le [long]$rows[$i-1].callback_count) { $stalls++ }
}
$duration=[double]$rows[-1].second-[double]$rows[0].second
$cpu=[double]$rows[-1].process_cpu_seconds-[double]$rows[0].process_cpu_seconds
$working=@($rows | ForEach-Object { [long]$_.working_set_bytes })
$private=@($rows | ForEach-Object { [long]$_.private_bytes })
$result=[ordered]@{
    observed_seconds=$duration
    process_cpu_seconds=$cpu
    cpu_percent_of_one_logical_core=100*$cpu/$duration
    working_set_max_bytes=($working | Measure-Object -Maximum).Maximum
    private_max_bytes=($private | Measure-Object -Maximum).Maximum
    private_start_bytes=$private[0]
    private_final_bytes=$private[-1]
    seconds_without_callback_progress=$stalls
    final_feature_drops=[long]$rows[-1].feature_drops
    final_driver_xruns=[long]$rows[-1].xruns
    final_bpm=[double]$rows[-1].bpm
    final_locked=[int]$rows[-1].locked
    note='No-progress watchdog is one-second resolution. Xrun -1 means unsupported, not zero. Physical loopback is not measured.'
}
$result | ConvertTo-Json | Set-Content -LiteralPath $Output -Encoding utf8
$result | ConvertTo-Json
