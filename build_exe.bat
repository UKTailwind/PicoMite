@echo off
pyinstaller --onefile --console ^
    --collect-all bleak ^
    --hidden-import winrt.windows.devices.bluetooth ^
    --hidden-import winrt.windows.devices.bluetooth.advertisement ^
    --hidden-import winrt.windows.devices.bluetooth.genericattributeprofile ^
    --hidden-import winrt.windows.devices.enumeration ^
    --hidden-import winrt.windows.foundation ^
    --hidden-import winrt.windows.foundation.collections ^
    --hidden-import winrt.windows.storage.streams ^
    --name ble_term ^
    ble_term.py
