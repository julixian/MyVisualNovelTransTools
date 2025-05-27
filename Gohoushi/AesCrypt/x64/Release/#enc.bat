for %%a in (gh_*) do .\Tool\AesCrypt.exe encrypt %%a %%a.enc -p 0123456789012345 %%a
pause