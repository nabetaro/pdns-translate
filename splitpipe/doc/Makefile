all: splitpipe.1 splitpipe.1.html joinpipe.1 joinpipe.1.html splitpipe.5.html \
	splitpipe.5

dist: all
	mkdir ../splitpipe-$(VERSION)/doc
	cp Makefile *.conf asciidoc.py *.txt *.html *.[15] ../splitpipe-$(VERSION)/doc	

# splitpipe.1.txt -> splitpipe.1.xml -> splitpipe.1

%.xml: %.txt
	./asciidoc.py -f ./asciidoc.conf -b docbook -d manpage  $<

%: %.xml
	xmlto man $<

%.html: %.txt
	asciidoc.py -f ./asciidoc.conf -b css-embedded -d manpage  $<	

clean:
	rm -f *.xml *.[15] *.html *~

