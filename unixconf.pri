menuicon.files = Resources/arpmanetdc.desktop
menuicon.path = $$PREFIX/share/applications/
INSTALLS += menuicon

icons.files = Resources/menuicons/hicolor/
icons.path = $$PREFIX/share/icons/
INSTALLS += icons

pixmap.files = Resources/menuicons/hicolor/arpmanetdc.xpm
pixmap.path = $$PREFIX/share/pixmaps/
INSTALLS += pixmap

pixmap-png.files = Resources/menuicons/hicolor/arpmanetdc.png
pixmap-png.path = $$PREFIX/share/pixmaps/
INSTALLS += pixmap-png

# INSTALL
target.path = $$PREFIX/bin/
INSTALLS += target
