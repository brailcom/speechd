<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
 <!ENTITY docbook.dsl SYSTEM 
	  "/usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/html/docbook.dsl" 
	  CDATA DSSSL>
]>

<style-specification id="my-docbook-html" use="docbook">

 ;; Generate HTML 4.0
(define %html40% #t)

;; adresáø, kam se mají ukládat vygenerované HTML soubory
(define %output-dir% "html")

;; má se pou¾ívat vý¹e uvedený adresáø
(define use-output-dir #t)

;; jméno hlavní stránky
(define %root-filename% "index")

;; mají se jména odvozovat z hodnoty atributu ID
(define %use-id-as-filename% #t)

;; pøípona pro HTML soubory
(define %html-ext% ".html")

;; do hlavièky stránky pøidáme informaci o pou¾itém kódování
(define %html-header-tags%
  '(("META"
     ("HTTP-EQUIV" "Content-Type")
     ("CONTENT" "text/html; charset=ISO-8859-1"))))

;; kde se mají geenrovat navigaèní odkazy
(define %header-navigation% #t)
(define %footer-navigation% #t)

;; Name of the stylesheet to use
;(define %stylesheet% "speechd.css")
;; The type of the stylesheet to use
;(define %stylesheet-type% "text/css")

</style-specification>

<external-specification id="docbook" document="docbook.dsl">