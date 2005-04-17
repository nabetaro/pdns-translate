all: splitpipe.1 splitpipe.1.html joinpipe.1 joinpipe.1.html splitpipe.5.html \
	splitpipe.5
	

# splitpipe.1.txt -> splitpipe.1.xml -> splitpipe.1

%.xml: %.txt
	./asciidoc.py -f ./asciidoc.conf -b docbook -d manpage  $<

%: %.xml
	xmlto man $<

%.html: %.txt
	asciidoc.py -f ./asciidoc.conf -b css-embedded -d manpage  $<	

clean:
	rm -f *.xml *.1 *.html *~

