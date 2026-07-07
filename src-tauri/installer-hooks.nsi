; Kill running Delta processes before installation to prevent file-lock errors
!macro NSIS_HOOK_PREINSTALL
  ExecWait 'cmd.exe /c taskkill /F /FI "IMAGENAME eq llama-server*" 2>nul'
  ExecWait 'cmd.exe /c taskkill /F /FI "IMAGENAME eq delta-server*" 2>nul'
  ExecWait 'cmd.exe /c taskkill /F /IM "Delta.exe" 2>nul'
  Sleep 2000
!macroend
