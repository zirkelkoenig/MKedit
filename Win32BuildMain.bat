@echo off
cl -Zi /LD Main.cpp /link /EXPORT:Setup /EXPORT:SetViewport
