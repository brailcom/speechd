;;; Extended support of token-to-word processing

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


(defvar ttw-token-to-words-funcs ()
  "List of functions to run on each token_to_words invocation.
Each of the functions must take three arguments: TOKEN, NAME, and FUNCTION.
FUNCTION is a function of two arguments TOKEN and NAME, that should be run by
the three argument function on each word it creates.  In such a way it's
ensured all the functions in the list are run sequentially.")

(defvar ttw-token-method-hook ()
  "List of functions to run after Token_Method is applied.
Each of the functions is a function of the single argument UTTERANCE, returning
the UTTERANCE.")


(defvar ttw-token-to-words.orig nil)


(define (ttw-token-to-words-function n)
  (let ((f (nth n ttw-token-to-words-funcs)))
    (if f
        (lambda (token name)
          (f token name (ttw-token-to-words-function (+ n 1))))
        ttw-token-to-words.orig)))

(define (ttw-token-to-words token name)
  ((ttw-token-to-words-function 0) token name))

(define (ttw-token-method utt)
  (apply_method 'Token_Method_orig utt)
  (apply_hooks ttw-token-method-hook utt)
  utt)

(define (ttw-setup)
  (if (not (eq? token_to_words ttw-token-to-words))
      (begin
        (set! ttw-token-to-words.orig token_to_words)
        (set! token_to_words ttw-token-to-words)))
  (if (not (eq? (Param.get 'Token_Method) ttw-token-method))
      (begin
        (Param.set 'Token_Method_orig (Param.get 'Token_Method))
        (Param.set 'Token_Method ttw-token-method))))


(provide 'ttw)
