SUBDIRS=chapter

EXTRA_DIST = \
	     lua-classes.dot \
	     architecture.dot \
	     architecture-overview.dot \
	     architecture.txt \
	     core.txt \
	     chassis.txt \
	     plugins.txt \
	     protocol.txt \
	     tests.txt \
	     scripting.txt \
	     lifecycle.msc

## we use http://docutils.sourceforge.net/rst.html to generate the docs

%.html: %.txt
	rst2html.py $< $@

book.html: book.txt chapter/scripting.txt chapter/protocol.txt

protocol.html: protocol.txt chapter/protocol.txt

scripting.html: scripting.txt chapter/scripting.txt

