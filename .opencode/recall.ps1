param(
  [int]$TodoId = -1
)

$stateFile = Join-Path $PSScriptRoot "state.json"
if (-not (Test-Path $stateFile)) {
  Write-Error "state.json not found. Run python setup.py to create it."
  exit 1
}

$state = Get-Content $stateFile | ConvertFrom-Json

if ($TodoId -ge 0) {
  $todo = $state.next_todos | Where-Object { $_.id -eq $TodoId }
  if (-not $todo) {
    Write-Error "Todo #$TodoId not found."
    exit 1
  }
  Write-Host "=== Todo #$($todo.id): $($todo.title) ===" -ForegroundColor Cyan
  Write-Host $todo.description
  Write-Host "Status: $($todo.status)"
  exit 0
}

Write-Host "=== Session: $($state.session) ===" -ForegroundColor Cyan
Write-Host "Branch: $($state.branch)"
Write-Host "Summary: $($state.summary)"
Write-Host ""
Write-Host "Changed files:" -ForegroundColor Yellow
$state.changed_files | ForEach-Object { Write-Host "  $_" }
Write-Host ""
Write-Host "Next Todos:" -ForegroundColor Green
$state.next_todos | ForEach-Object {
  $color = switch ($_.priority) {
    "high" { "Red" }
    "medium" { "Yellow" }
    "low" { "Gray" }
  }
  $mark = switch ($_.status) {
    "pending" { "[ ]" }
    "in_progress" { "[>]" }
    "completed" { "[x]" }
  }
  Write-Host "  $mark #$($_.id) $($_.title)" -ForegroundColor $color
}
Write-Host ""
Write-Host "Usage:" -ForegroundColor DarkGray
Write-Host "  .opencode\recall.ps1          - Show all" -ForegroundColor DarkGray
Write-Host "  .opencode\recall.ps1 -TodoId N - Show detail for todo #N" -ForegroundColor DarkGray
