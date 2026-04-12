# Self-signed certificate generation script
# Requires administrator privileges

param(
    [string]$CertName = "PrintDriver",
    [string]$OutputPath = ".\cert"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "PrintDriver Certificate Generator" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

# Create output directory
if (-not (Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath | Out-Null
}

# Check for existing certificate
$existingCert = Get-ChildItem -Path Cert:\LocalMachine\My | Where-Object { $_.Subject -like "*CN=$CertName*" }

if ($existingCert) {
    Write-Host "Found existing certificate: $($existingCert.Thumbprint)" -ForegroundColor Yellow
    $choice = Read-Host "Use existing certificate? (Y/N)"
    if ($choice -eq 'Y' -or $choice -eq 'y') {
        $cert = $existingCert[0]
    } else {
        Write-Host "Removing old certificate..."
        Remove-Item -Path "Cert:\LocalMachine\My\$($existingCert.Thumbprint)" -Force
        $existingCert = $null
    }
}

if (-not $existingCert) {
    Write-Host "`nCreating new code signing certificate..." -ForegroundColor Green
    
    $cert = New-SelfSignedCertificate `
        -Type CodeSigningCert `
        -Subject "CN=$CertName" `
        -FriendlyName "$CertName Code Signing Certificate" `
        -CertStoreLocation "Cert:\LocalMachine\My" `
        -KeyUsage DigitalSignature `
        -KeyLength 2048 `
        -NotAfter (Get-Date).AddYears(10)
    
    if ($cert) {
        Write-Host "Certificate created: $($cert.Thumbprint)" -ForegroundColor Green
    } else {
        Write-Host "Failed to create certificate" -ForegroundColor Red
        exit 1
    }
}

# Export certificate
$cerPath = Join-Path $OutputPath "$CertName.cer"
Export-Certificate -Cert $cert -FilePath $cerPath -Type CERT | Out-Null
Write-Host "Certificate exported: $cerPath" -ForegroundColor Green

# Export PFX file
$pfxPath = Join-Path $OutputPath "$CertName.pfx"
$pfxPassword = ConvertTo-SecureString -String "PrintDriver2024" -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $pfxPassword | Out-Null
Write-Host "PFX exported: $pfxPath" -ForegroundColor Green
Write-Host "PFX Password: PrintDriver2024" -ForegroundColor Yellow

# Import certificate to Trusted Publishers
Write-Host "`nImporting certificate to Trusted Publishers..." -ForegroundColor Green
try {
    Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher" | Out-Null
    Write-Host "Certificate imported to Trusted Publishers" -ForegroundColor Green
} catch {
    Write-Host "Import failed. Please run as administrator." -ForegroundColor Red
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Certificate generation complete!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Files:" -ForegroundColor White
Write-Host "  - $cerPath (for installer)" -ForegroundColor White
Write-Host "  - $pfxPath (for signing)" -ForegroundColor White
