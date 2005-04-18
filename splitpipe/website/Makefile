all: index.html downloads.html documentation.html development.html faq.html bugs.html roadmap.html examples.html

%.html: %.txt
	asciidoc.py -b css -f asciidoc.conf  $<	

clean:
	rm -f  *.html *~

publish:
	rsync -avzre ssh --exclude=.svn . ds9a.nl:/var/www/ds9a.nl/splitpipe
