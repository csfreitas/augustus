param([Parameter(Mandatory)][string]$SourceRoot,[Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
# Copy complete, unmodified production functions. This diagnostic avoids linking
# graphics/audio and all unrelated save restoration consumers into a console tool.
function Extract-Functions($relative, $firstFunction, $names, $output) {
    $text = Get-Content -LiteralPath (Join-Path $SourceRoot $relative) -Raw
    $matches = [regex]::Matches($text, '(?ms)^[\w *]+\b(\w+)\([^;{}]*\)\s*\{.*?^\}')
    $first = @($matches | Where-Object { $_.Groups[1].Value -eq $firstFunction })
    if ($first.Count -ne 1) { throw "Missing first function: $firstFunction" }
    $prefix = $text.Substring(0,$first[0].Index)
    $prefix = $prefix.Replace('#include "file_io.h"','#include "game/file_io.h"').Replace('#include "data.h"','#include "city/data.h"').Replace('#include "scenario.h"','#include "scenario/scenario.h"')
    $selected = @($matches | Where-Object { $_.Groups[1].Value -in $names })
    if ($selected.Count -ne $names.Count) { throw "Unexpected extraction count for $relative" }
    [IO.File]::WriteAllText((Join-Path $Destination $output), $prefix + "`n" + (($selected | ForEach-Object Value) -join "`n`n"))
}
Extract-Functions 'src/game/file_io.c' 'init_file_piece' @('init_file_piece','create_savegame_piece','clear_savegame_pieces','get_version_data','init_savegame_data','read_int32','read_compressed_chunk','read_compressed_savegame_chunk','prepare_dynamic_piece_from_file','savegame_read_from_file','get_savegame_versions') 'save_decoder.c'
Extract-Functions 'src/city/data.c' 'city_data_init' @('load_main_data','load_entry_exit','city_data_load_state') 'city_decoder.c'
Extract-Functions 'src/scenario/scenario.c' 'calculate_buffer_offsets' @('calculate_buffer_offsets','scenario_get_state_buffer_size_by_savegame_version') 'scenario_sizes.c'
