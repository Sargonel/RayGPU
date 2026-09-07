# A localhost-only static server using Windows' built-in .NET runtime.
param([int]$Port = 8000)
$ErrorActionPreference = 'Stop'
$webRoot = Join-Path $PSScriptRoot 'build/web'
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $Port)
$mimeTypes = @{
    '.html'='text/html; charset=utf-8'; '.js'='text/javascript; charset=utf-8'
    '.wasm'='application/wasm'; '.png'='image/png'; '.jpg'='image/jpeg'; '.jpeg'='image/jpeg'
    '.bmp'='image/bmp'; '.gif'='image/gif'; '.tga'='image/x-tga'; '.wav'='audio/wav'
}
$resolvedWebRoot = [System.IO.Path]::GetFullPath($webRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar)
$listener.Start()
Write-Host "raygpu: http://127.0.0.1:$Port (Ctrl+C to stop)"
$script:stopServer = $false
$consoleInput = -not [Console]::IsInputRedirected
$previousControlC = $false
if ($consoleInput) {
    $previousControlC = [Console]::TreatControlCAsInput
    [Console]::TreatControlCAsInput = $true
}
function Test-ServerStop {
    if ($consoleInput) {
        while ([Console]::KeyAvailable) {
            $key = [Console]::ReadKey($true)
            if ($key.KeyChar -eq [char]3) { $script:stopServer = $true }
        }
    }
    return $script:stopServer
}
# Yield to PowerShell while waiting so it can process Ctrl+C. Blocking .NET
# AcceptTcpClient/ReadLine calls otherwise defer pipeline cancellation.
function Read-RequestLine($Reader, $Deadline) {
    $read = $Reader.ReadLineAsync()
    while (-not $read.IsCompleted) {
        if (Test-ServerStop) { throw 'Server stopped' }
        if ([DateTime]::UtcNow -ge $Deadline) { throw 'Request timed out' }
        Start-Sleep -Milliseconds 50
    }
    return $read.GetAwaiter().GetResult()
}
try {
    while (-not (Test-ServerStop)) {
        while (-not $listener.Pending() -and -not (Test-ServerStop)) { Start-Sleep -Milliseconds 50 }
        if ($script:stopServer) { break }
        $client = $listener.AcceptTcpClient()
        try {
            $client.ReceiveTimeout = 3000
            $client.SendTimeout = 3000
            $stream = $client.GetStream()
            $reader = [System.IO.StreamReader]::new($stream)
            $deadline = [DateTime]::UtcNow.AddSeconds(3)
            $request = Read-RequestLine $reader $deadline
            if (-not $request) { continue }
            do { $header = Read-RequestLine $reader $deadline } while ($header)
            $parts = $request.Split(' ')
            $code = '404 Not Found'
            $mime = 'text/plain'
            $body = [System.Text.Encoding]::UTF8.GetBytes('Not found')
            if ($parts[0] -eq 'GET') {
                $requestPath = [System.Uri]::UnescapeDataString($parts[1].Split('?')[0])
                if ($requestPath -eq '/') { $requestPath = '/index.html' }
                $relativePath = $requestPath.TrimStart('/').Replace('/', [System.IO.Path]::DirectorySeparatorChar)
                $path = [System.IO.Path]::GetFullPath((Join-Path $resolvedWebRoot $relativePath))
                $insideRoot = $path.StartsWith($resolvedWebRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
                if ($insideRoot -and (Test-Path -LiteralPath $path -PathType Leaf)) {
                    $body = [System.IO.File]::ReadAllBytes($path)
                    $extension = [System.IO.Path]::GetExtension($path).ToLowerInvariant()
                    $mime = $mimeTypes[$extension]
                    if (-not $mime) { $mime = 'application/octet-stream' }
                    $code = '200 OK'
                }
            }
            $response = "HTTP/1.1 $code`r`nContent-Type: $mime`r`nContent-Length: $($body.Length)`r`nCache-Control: no-store`r`nConnection: close`r`n`r`n"
            $bytes = [System.Text.Encoding]::ASCII.GetBytes($response)
            $stream.Write($bytes, 0, $bytes.Length)
            $stream.Write($body, 0, $body.Length)
        } catch { Write-Verbose $_ } finally { $client.Dispose() }
    }
} finally {
    $listener.Stop()
    if ($consoleInput) { [Console]::TreatControlCAsInput = $previousControlC }
    Write-Host 'Server stopped.'
}
