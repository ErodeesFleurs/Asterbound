#!/bin/sh -e

cd "`dirname \"$0\"`"

osascript <<END 

tell application "Terminal"
  do script "cd \"`pwd`\";./asterbound_server $@;exit"
end tell

END
