<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
 <!ENTITY docbook.dsl SYSTEM 
	  "/usr/lib/sgml/stylesheet/dsssl/docbook/nwalsh/html/docbook.dsl" 
	  CDATA DSSSL>
]>

<style-specification id="my-docbook-html" use="docbook">

;; Generate HTML 4.0
(define %html40% #t)

;; Output directory for generated HTML files
;(define %output-dir% "html")

;; Really use output directory defined above?
;(define use-output-dir #t)

;; Name of the documentation root file.
(define %root-filename% "index")

;; Should filenemes be derived from "id" attributte of chapters?
(define %use-id-as-filename% #t)

;; Extension of HTML files
(define %html-ext% ".html")

;; Add meta tags to HTML header.
(define %html-header-tags%
  '(("META"
     ("HTTP-EQUIV" "Content-Type")
     ("CONTENT" "text/html; charset=ISO-8859-1"))))

;; Where to generate the navigation links.
(define %header-navigation% #t)
(define %footer-navigation% #t)

;; Name of the stylesheet to use
;(define %stylesheet% "speechd.css")
;; The type of the stylesheet to use
;(define %stylesheet-type% "text/css")

</style-specification>

<external-specification id="docbook" document="docbook.dsl">