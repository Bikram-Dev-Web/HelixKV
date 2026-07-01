# Interactive HelixKV PowerShell Client
$port = 6379
$server = "127.0.0.1"

Write-Host "Connecting to HelixKV at ${server}:${port}..." -ForegroundColor Cyan

try {
    $client = New-Object System.Net.Sockets.TcpClient($server, $port)
    $stream = $client.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream)
    $reader = New-Object System.IO.StreamReader($stream)
    $writer.AutoFlush = $true

    Write-Host "Connected successfully!" -ForegroundColor Green
    Write-Host "You can type commands like 'SET key value', 'GET key', 'DEL key', 'EXISTS key', 'KEYS', 'SIZE', 'CLEAR', 'INFO', or 'PING'." -ForegroundColor Gray
    Write-Host "Type 'exit' to quit." -ForegroundColor Gray
    Write-Host ""

    while ($true) {
        # Prompt the user
        $cmd = Read-Host "helixkv"
        
        if ($cmd -eq "exit") {
            break
        }
        if ([string]::IsNullOrWhiteSpace($cmd)) {
            continue
        }

        # Send command to server
        $writer.WriteLine($cmd)

        # Read response from server
        $response = $reader.ReadLine()
        
        # Display response
        if ($response -like "ERR*") {
            Write-Host $response -ForegroundColor Red
        } elseif ($response -eq "OK" -or $response -eq "DELETED" -or $response -eq "PONG") {
            Write-Host $response -ForegroundColor Green
        } else {
            Write-Host $response
        }
    }

    $client.Close()
    Write-Host "Disconnected." -ForegroundColor Yellow
}
catch {
    Write-Host "Connection failed. Please ensure the server is running on port $port." -ForegroundColor Red
    Write-Error $_
}
