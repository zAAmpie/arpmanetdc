menuicon.files = Resources/menuicons/arpmanetdc.desktop
menuicon.path = $$PREFIX/share/applications/
INSTALLS += menuicon

icons.files = Resources/menuicons/
icons.path = $$PREFIX/share/icons/hicolor/
INSTALLS += icons

pixmap.files = Resources/menuicons/arpmanetdc.png
pixmap.path = $$PREFIX/share/pixmaps/
INSTALLS += pixmap

# INSTALL
target.path = $$PREFIX/bin/
INSTALLS += target
