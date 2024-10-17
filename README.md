# LotusAppClient
**BUILD INSTRUCTIONS**

1. Pull repo
2. Install QT: brew install Qt
3. make clean / make
4. if 3 doesnt work, run this first:
*/opt/homebrew/bin/qmake /Users/PATH_TO_FOLDER/LotusAppClient/ChatApplication.pro -spec macx-clang CONFIG+=debug CONFIG+=qml_debug && /usr/bin/make qmake_all*
5. Run with: *./ChatApplication.app/Contents/MacOS/ChatApplication*
