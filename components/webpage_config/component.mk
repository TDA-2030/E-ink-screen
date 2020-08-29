#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)


COMPONENT_ADD_INCLUDEDIRS := include
COMPONENT_SRCDIRS := . cgi adapt

COMPONENT_EMBED_FILES := html/cfg_favicon.ico
COMPONENT_EMBED_FILES += html/medley.min.js
COMPONENT_EMBED_FILES += html/connecting.html
COMPONENT_EMBED_FILES += html/style.css
COMPONENT_EMBED_FILES += html/wifi.png
COMPONENT_EMBED_FILES += html/wifi.html
