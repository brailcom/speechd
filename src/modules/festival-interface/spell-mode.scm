;;; Spelling mode

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


(defvar mode nil)
(defvar orig-mode nil)

(defvar spell-orig-token.singlecharsymbols nil)

(define (spell_init_func)
  (set! spell-orig-token.singlecharsymbols token.singlecharsymbols)
  (set! token.singlecharsymbols "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz¡¢£¤¥¦§¨©ª«¬­®¯°±²³´µ¶·¸¹º»¼½¾¿ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏĞÑÒÓÔÕÖ×ØÙÚÛÜİŞßàáâãäåæçèéêëìíîïğñòóôõö÷øùúûüış")
  (set! orig-mode mode)
  (set! mode 'spell))

(define (spell_exit_func)
  (set! token.singlecharsymbols spell-orig-token.singlecharsymbols)
  (set! mode orig-mode))

(set! tts_text_modes
      (cons
       (list
        'spell
        (list
         (list 'init_func spell_init_func)
         (list 'exit_func spell_exit_func)))
       tts_text_modes))

(provide 'spell-mode)
