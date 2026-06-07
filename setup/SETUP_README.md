## Passo 1: Instalar o MSYS2 pelo Terminal adm (Windows)

winget install MSYS2.MSYS2

## Passo 2: Instalar o Compilador G++ com Pthreads

pacman -S --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake

## Passo 3: Configurar as Variáveis de Ambiente (PATH)

[Environment]::SetEnvironmentVariable('Path', [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';C:\msys64\ucrt64\bin', 'Machine')

## Passo 4: Feche e reabra o vscode

## Passo 5: Testes
#### Siga os testes "test_cpp.cpp e test_pthread.cpp"