[CmdletBinding()]
param(
    [string]$ReleaseVersion
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function ConvertTo-Offset {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Value,
        [Parameter(Mandatory = $true)]
        [string]$Description
    )

    $valueAsText = [string]$Value
    if ($valueAsText -notmatch '^0x[0-9a-fA-F]+$') {
        throw "$Description muss als Hexadezimalwert im Format 0x... angegeben werden."
    }

    return [Convert]::ToInt64($valueAsText.Substring(2), 16)
}

function Test-ReleaseManifest {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ManifestPath,
        [Parameter(Mandatory = $true)]
        [string]$ExpectedVersion
    )

    $manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
    $releaseDirectory = Split-Path -Parent $ManifestPath

    foreach ($propertyName in @('schemaVersion', 'product', 'productName', 'version', 'releasedAt', 'targetChip', 'status', 'gitCommit', 'files')) {
        if (-not $manifest.PSObject.Properties.Name.Contains($propertyName)) {
            throw "Manifest $ManifestPath enthält das Pflichtfeld '$propertyName' nicht."
        }
    }

    if ($manifest.schemaVersion -ne 1 -or $manifest.product -ne 'OneKlickPony') {
        throw "Manifest $ManifestPath hat keine unterstützte Schema- oder Produktkennung."
    }
    if ($manifest.version -ne $ExpectedVersion -or $manifest.version -notmatch '^\d+\.\d+\.\d+$') {
        throw "Manifest $ManifestPath enthält keine passende Versionsnummer."
    }
    if ($manifest.releasedAt -notmatch '^\d{2}\.\d{2}\.\d{4}$') {
        throw "Manifest $ManifestPath enthält kein Datum im Format TT.MM.JJJJ."
    }
    if ($manifest.targetChip -ne 'ESP32-C3' -or $manifest.status -notin @('stable', 'testing')) {
        throw "Manifest $ManifestPath enthält ein ungültiges Zielgerät oder einen ungültigen Freigabestatus."
    }
    if ($manifest.gitCommit -notmatch '^[0-9a-f]{40}$') {
        throw "Manifest $ManifestPath enthält keinen vollständigen Git-Commit."
    }
    if (@($manifest.files).Count -eq 0) {
        throw "Manifest $ManifestPath enthält keine Flash-Dateien."
    }

    $writtenRanges = @()
    foreach ($file in @($manifest.files)) {
        foreach ($propertyName in @('path', 'offset', 'size', 'sha256')) {
            if (-not $file.PSObject.Properties.Name.Contains($propertyName)) {
                throw "Manifest $ManifestPath enthält bei einer Datei das Pflichtfeld '$propertyName' nicht."
            }
        }

        if ($file.path -match '[\\/]' -or [IO.Path]::GetFileName($file.path) -ne $file.path) {
            throw "Manifest $ManifestPath enthält einen unsicheren Dateinamen '$($file.path)'."
        }
        if ($file.size -le 0 -or $file.sha256 -notmatch '^[0-9a-f]{64}$') {
            throw "Manifest $ManifestPath enthält ungültige Metadaten für '$($file.path)'."
        }

        $offset = ConvertTo-Offset -Value $file.offset -Description "Der Offset von '$($file.path)'"
        $artifactPath = Join-Path $releaseDirectory $file.path
        if (-not (Test-Path -LiteralPath $artifactPath -PathType Leaf)) {
            throw "Freigabedatei fehlt: $artifactPath"
        }

        $artifact = Get-Item -LiteralPath $artifactPath
        if ($artifact.Length -ne [Int64]$file.size) {
            throw "Dateigröße von '$($file.path)' stimmt nicht mit dem Manifest überein."
        }
        $hash = (Get-FileHash -LiteralPath $artifactPath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($hash -ne $file.sha256) {
            throw "SHA-256-Prüfsumme von '$($file.path)' stimmt nicht mit dem Manifest überein."
        }

        $writtenRanges += [PSCustomObject]@{
            Name = $file.path
            Start = $offset
            End = $offset + [Int64]$file.size
        }
    }

    foreach ($range in $writtenRanges) {
        foreach ($otherRange in $writtenRanges) {
            if ($range.Name -lt $otherRange.Name -and $range.Start -lt $otherRange.End -and $otherRange.Start -lt $range.End) {
                throw "Flash-Bereiche '$($range.Name)' und '$($otherRange.Name)' überlappen."
            }
        }
    }

    foreach ($region in @($manifest.preservedRegions)) {
        foreach ($propertyName in @('name', 'offset', 'size')) {
            if (-not $region.PSObject.Properties.Name.Contains($propertyName)) {
                throw "Manifest $ManifestPath enthält einen unvollständigen geschützten Bereich."
            }
        }

        $regionStart = ConvertTo-Offset -Value $region.offset -Description "Der Offset des geschützten Bereichs '$($region.name)'"
        $regionEnd = $regionStart + (ConvertTo-Offset -Value $region.size -Description "Die Größe des geschützten Bereichs '$($region.name)'")
        foreach ($range in $writtenRanges) {
            if ($range.Start -lt $regionEnd -and $regionStart -lt $range.End) {
                throw "Flash-Datei '$($range.Name)' überschreibt den geschützten Bereich '$($region.name)'."
            }
        }
    }

    $checksumsPath = Join-Path $releaseDirectory 'SHA256SUMS.txt'
    if (-not (Test-Path -LiteralPath $checksumsPath -PathType Leaf)) {
        throw "SHA256SUMS.txt fehlt neben dem Manifest $ManifestPath."
    }
    $checksums = Get-Content -LiteralPath $checksumsPath
    foreach ($file in @($manifest.files)) {
        $expectedLine = "$($file.sha256) *$($file.path)"
        if ($checksums -notcontains $expectedLine) {
            throw "SHA256SUMS.txt enthält keine passende Prüfsumme für '$($file.path)'."
        }
    }

    Write-Output "Freigabe $ExpectedVersion ist gültig: $ManifestPath"
}

$projectDirectory = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$firmwareDirectory = Join-Path $projectDirectory 'web\public\firmware'
$catalogPath = Join-Path $firmwareDirectory 'catalog.json'

if (-not (Test-Path -LiteralPath $catalogPath -PathType Leaf)) {
    throw "Firmwarekatalog fehlt: $catalogPath"
}

$catalog = Get-Content -Raw -LiteralPath $catalogPath | ConvertFrom-Json
if ($catalog.schemaVersion -ne 1 -or $catalog.product -ne 'OneKlickPony' -or @($catalog.releases).Count -eq 0) {
    throw "Firmwarekatalog enthält keine unterstützte Produktfreigabe."
}

$seenVersions = @{}
$releasesToValidate = @($catalog.releases)
if ($ReleaseVersion) {
    $releasesToValidate = @($catalog.releases | Where-Object { $_.version -eq $ReleaseVersion })
    if ($releasesToValidate.Count -ne 1) {
        throw "Im Katalog wurde keine eindeutige Freigabe für Version $ReleaseVersion gefunden."
    }
}

foreach ($release in @($catalog.releases)) {
    if ($seenVersions.ContainsKey($release.version)) {
        throw "Der Firmwarekatalog enthält die Version '$($release.version)' mehrfach."
    }
    $seenVersions[$release.version] = $true
}

foreach ($release in $releasesToValidate) {
    foreach ($propertyName in @('productName', 'version', 'releasedAt', 'targetChip', 'status', 'manifest', 'releaseNotes')) {
        if (-not $release.PSObject.Properties.Name.Contains($propertyName)) {
            throw "Firmwarekatalog enthält bei einer Freigabe das Pflichtfeld '$propertyName' nicht."
        }
    }
    if ($release.version -notmatch '^\d+\.\d+\.\d+$' -or $release.releasedAt -notmatch '^\d{2}\.\d{2}\.\d{4}$') {
        throw "Firmwarekatalog enthält eine ungültige Version oder ein ungültiges Datum."
    }
    if ($release.targetChip -ne 'ESP32-C3' -or $release.status -notin @('stable', 'testing')) {
        throw "Firmwarekatalog enthält ein ungültiges Zielgerät oder einen ungültigen Freigabestatus."
    }
    if ($release.manifest -notmatch '^releases/[0-9]+\.[0-9]+\.[0-9]+/manifest\.json$') {
        throw "Firmwarekatalog enthält einen ungültigen Manifestpfad."
    }
    if ($release.releaseNotes.PSObject.Properties.Name -notcontains 'de' -or $release.releaseNotes.PSObject.Properties.Name -notcontains 'en') {
        throw "Firmwarekatalog enthält keine zweisprachigen Freigabehinweise."
    }

    $manifestPath = Join-Path $firmwareDirectory ($release.manifest -replace '/', [IO.Path]::DirectorySeparatorChar)
    Test-ReleaseManifest -ManifestPath $manifestPath -ExpectedVersion $release.version
}

Write-Output 'Firmwarekatalog ist gültig. Es wurde kein PlatformIO-Build ausgeführt.'
