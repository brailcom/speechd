;;; Package definition

;; Author: Milan Zamazal <pdm@brailcom.org>

;; Copyright (C) 2004 Brailcom, o.p.s.

;; COPYRIGHT NOTICE

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU Lesser General Public License as published by
;; the Free Software Foundation; either version 2.1 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
;; or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
;; for more details.

;; You should have received a copy of the GNU Lesser General Public License
;; along with this program.  If not, see <https://www.gnu.org/licenses/>.


(in-package :cl-user)


(defpackage :ssip
  (:use :cl)
  (:export
   ;; configuration.lisp
   #:*application-name*
   #:*client-name*
   #:*language*
   #:*spell*
   ;; ssip.lisp
   #:connection-names
   #:set-language
   #:open-connection
   #:close-connection
   #:reopen-connection
   #:say-text
   #:say-sound
   #:say-char
   #:cancel
   #:stop
   #:pause
   #:resume
   ))

