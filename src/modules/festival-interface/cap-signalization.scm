;;; Capital character signalization

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


(require 'util)
(require 'ttw)


(defvar cap-signalization-mode nil)

(define (cap-token-to-words token name ttw)
  (if cap-signalization-mode
      (let ((ttw* (lambda (token name)
                    (append (if (string-matches name "^[A-Z¡»œ…ÃÕ“”ÿ©´⁄Ÿ›Æ].*")
                                (list '((name "") (capital-event ""))))
                            (ttw token name)))))
        (if (string-matches name "^..*[A-Z¡»œ…ÃÕ“”ÿ©´⁄Ÿ›Æ].*")
            (let ((i 1))
              (while (not (string-matches (substring name i 1)
                                          "^[A-Z¡»œ…ÃÕ“”ÿ©´⁄Ÿ›Æ].*"))
                     (set! i (+ i 1)))
              (append (ttw* token (substring name 0 i))
                      (cap-token-to-words
                       token
                       (substring name i (- (length name) i))
                       ttw)))
            (ttw* token name)))
      (ttw token name)))

(define (cap-mark-items utt)
  (mapcar
    (lambda (w)
      (if (item.has_feat w 'capital-event)
          (item.set_feat w 'event '(logical capital))))
    (utt.relation.items utt 'Word))
  utt)


(define (setup-cap-signalization)
  (add-hook ttw-token-to-words-funcs cap-token-to-words t)
  (add-hook ttw-token-method-hook cap-mark-items nil)
  (ttw-setup))
  
(define (set-cap-signalization-mode mode)
  "(set-cap-signalization-mode MODE)
Enable or disable capital letter signalization mode.
If MODE is non-nil, enable the mode, otherwise disable it."
  (setup-cap-signalization)
  (set! cap-signalization-mode mode))


(provide 'cap-signalization)
