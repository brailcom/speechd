;;; Miscellaneous utilities

;; Copyright (C) 2003 Brailcom, o.p.s.

;; Author: Milan Zamazal <pdm@brailcom.org>

;; COPYRIGHT NOTICE

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
;; or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
;; for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program; if not, write to the Free Software
;; Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.


;;; Commonly used Lisp constructs

(defmac (when form)
  `(if ,(cadr form)
       (begin
         ,@(cddr form))))

(defmac (unless form)
  `(if (not ,(cadr form))
       (begin
         ,@(cddr form))))

(defmac (prog1 form)
  `(let ((result ,(cadr form)))
     ,@(cddr form)
     result))

(defmac (unwind-protect* form)
  (let ((protected-form (nth 1 form))
        (cleanup-forms (nth_cdr 2 form)))
    `(unwind-protect
       (prog1 ,protected-form
         ,@cleanup-forms)
       (begin
         ,@cleanup-forms))))

(define (first list)
  (car list))

(define (second list)
  (cadr list))

(define (identity x)
  x)

;;; General utilities

(defmac (add-hook form)
  (let ((hook-var (nth 1 form))
        (hook (nth 2 form))
        (to-end? (nth 3 form)))
    `(if (member ,hook ,hook-var)
         ,hook-var
         (set! ,hook-var (if ,to-end?
                             (append ,hook-var (list ,hook))
                             (cons ,hook ,hook-var))))))

;;; Festival specific utilities

(define (item.has_feat item feat)
  (assoc feat (item.features item)))

(define (concat-waves waves)
  (let ((first-wave (car waves)))
    (if (<= (length waves) 1)
        first-wave
        (wave.append first-wave (concat-waves (cdr waves))))))

(define (langvar symbol)
  (let ((lsymbol (intern (string-append symbol "." (Param.get 'Language)))))
    (print lsymbol)
    (symbol-value (if (symbol-bound? lsymbol) lsymbol symbol))))


(provide 'util)
