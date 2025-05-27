for %%a in (gh_*) do .\Tool\AesCrypt.exe decrypt %%a %%a.dec -p 0123456789012345 %%a
pause