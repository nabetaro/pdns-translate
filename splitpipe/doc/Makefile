all: splitpipe.1 splitpipe.1.html joinpipe.1 joinpipe.1.html splitpipe.5.html \
	splitpipe.5

dist: all
	mkdir ../splitpipe-$(VERSION)/doc
	cp Makefile *.txt *.html *.[15] ../splitpipe-$(VERSION)/doc	

install: all
	@mkdir -p $(MANDIR)/man1/ $(MANDIR)/man5
	install splitpipe.1 $(MANDIR)/man1/
	install joinpipe.1 $(MANDIR)/man1/
	install splitpipe.5 $(MANDIR)/man5/

# splitpipe.1.txt -> splitpipe.1.xml -> splitpipe.1

%.xml: %.txt
	asciidoc.py -b docbook -d manpage  $<

%: %.xml
	xmlto man $<

%.html: %.txt
	asciidoc.py -b css-embedded -d manpage  $<	

clean:
	rm -f *.xml *.[15] *.html *~

