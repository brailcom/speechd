;;; Support of miscellaneous kinds of speech events

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
(require 'punctuation)


(defvar sound-icon-directory "/usr/share/sounds/sound-icons"
  "Directory where sound icons are placed by default.")

(defvar logical-event-mapping
  '((capital sound "percussion-12.wav")
    (empty-text sound "gummy-cat-2.wav"))
  "Alist mapping logical sound events to any events.
Each element of the alist is of the form (LOGICAL-EVENT EVENT-TYPE
EVENT-VALUE), where LOGICAL-EVENT is the symbol naming the event to transform,
EVENT-TYPE is the symbol naming the type of the transformed event and
EVENT-VALUE is the corresponding transformed event value.
The following event types are supported:
`logical' -- just this event type again, the value is a symbol naming the event
`text' -- plain text (the event value) to be synthesized
`sound' -- a WAV file to be played, the value is a string naming the file;
  either as an absolute pathname starting with the slash character or a
  pathname relative to `sound-icon-directory'
`key' -- a key event, the value is a string naming the key
`character' -- a character event, the value is a string naming the character
")

(defvar key-event-mapping ()
  "Alist mapping key events to any events.
The form of the alist is the same as in `logical-event-mapping', except
LOGICAL-EVENT is replaced by a string naming the key.")

(defvar character-event-mapping ()
  "Alist mapping character events to any events.
The form of the alist is the same as in `logical-event-mapping', except
LOGICAL-EVENT is replaced by a string naming the character.")

(defvar event-mappings
  (list
   (list 'logical logical-event-mapping)
   (list 'key key-event-mapping)
   (list 'character character-event-mapping))
  "Alist mapping event types to new events.
Each element of the alist is of the form (EVENT-TYPE EVENT-MAPPING), where
EVENT-TYPE is one of the symbols `logical', `text', `sound', `key',
`character', and EVENT-MAPPING is the of the same form as
`logical-event-mapping'.")

(defvar word-mapping
  '(("(" sound "guitar-13.wav")
    (")" sound "guitar-12.wav")
    ("@" sound "cembalo-6.wav"))
  "Alist mapping words to events.
Each element of the list is of the form (WORD EVENT-TYPE EVENT-VALUE), where
WORD is a string representing a word or a punctuation character and EVENT-TYPE
and EVENT-VALUE are the same as in `logical-event-mapping'.")


(defvar synth-event-result-format nil)


(define (event-mark-items utt)
  (mapcar
    (lambda (w)
      (let ((event (cdr (assoc (item.name w) word-mapping))))
        (if (and event
                 (not (item.has_feat w 'event)))
            (item.set_feat w 'event event))))
    (utt.relation.items utt 'Word)))

(define (event-split-items-1 utt w items)
  (cond
   ((not w)
    (append (if items (list (reverse items)))))
   ((item.has_feat w 'event)
    (append (if items (list (reverse items)))
            (list (list w))
            (event-split-items-1 utt (item.next w) ())))
   (t
    (event-split-items-1 utt (item.next w) (cons w items)))))

(define (event-split-items utt)
  (event-split-items-1 utt (utt.relation.first utt 'Word) '()))

(define (event-rest-of-synth utt)  
  (POS utt)
  (Phrasify utt)
  (Word utt)
  (Pauses utt)
  (Intonation utt)
  (PostLex utt)
  (Duration utt)
  (Int_Targets utt)
  (Wave_Synth utt)
  (apply_hooks after_synth_hooks utt)
  utt)

(define (event-synth-words utt word-items)
  (let ((event (and (item.has_feat (car word-items) 'event)
                    (item.feat (car word-items) 'event))))
    (if event
        (synth-event (car event) (cadr event))
        (let ((u (Utterance Text "")))
          (utt.relation.create u 'Token)
          (utt.relation.create u 'Word)
          (mapcar
            (lambda (w)
              (let ((tw (item.relation w 'Token)))
                (let ((token (item.root tw))
                      (ptw (item.prev tw)))
                  (if (and ptw (equal? token (item.root ptw)))
                      (set! token (utt.relation.last utt 'Token))
                      (utt.relation.append u 'Token token))
                  (utt.relation.append u 'Word w)
                  (item.append_daughter token tw))))
            word-items)
          (let ((result (event-rest-of-synth u)))
            (if synth-event-result-format
                (list result)
                result))))))

(define (event-synth-text text)
  (let ((utt (eval `(Utterance Text ,text))))
    (Text utt)
    (Token_POS utt)
    (Token utt)
    (event-mark-items utt)
    (let ((items (event-split-items utt)))
      (if (or (> (length items) 1)
              (item.has_feat (caar items) 'event))
          (if synth-event-result-format
              (apply append (mapcar (lambda (i) (event-synth-words utt i))
                                    items))
              (let ((u (Utterance Wave nil)))
                (utt.relation.create u 'Wave)
                (utt.relation.append
                 u 'Wave
                 `(what-should-be-here?
                   ((name "wave")
                    (wave ,(concat-waves
                            (mapcar
                             (lambda (its)
                               (utt.wave (event-synth-words utt its)))
                             items))))))
                u))
          (let ((result (utt.synth utt)))
            (if synth-event-result-format
                (list result)
                result))))))

(define (synth-event-plain type value)
  (cond
   ((eq? type 'text)
    (event-synth-text value))
   ((eq? type 'sound)
    (utt.import.wave (Utterance Wave nil)
                     (if (string-matches value "^/.*")
                         value
                         (string-append sound-icon-directory "/" value))))
   (t
    (let ((transformed (cdr (assoc value (cadr (assq type event-mappings))))))
      (cond
       (transformed
        (apply synth-event transformed))
       ((or (eq? type 'key) (eq? type 'character))
        (let ((pmode punctuation-mode))
          (set-punctuation-mode 'all)
          (unwind-protect
              (prog1 (SynthText value)
                (set-punctuation-mode pmode))
            (set-punctuation-mode pmode))))
       (t
        (error "Event description not found" (cons type value))))))))

(define (synth-event type value)
  (let ((result (synth-event-plain type value)))
    (if (or (not synth-event-result-format) (consp result))
        result
        (list result))))

(define (play-event type value)
  (let ((utt (synth-event type value)))
    (if synth-event-result-format
        (mapcar utt.play utt)
        (utt.play utt))))


(provide 'events)
