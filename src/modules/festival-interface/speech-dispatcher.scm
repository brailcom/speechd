;;; Speech Dispatcher interface

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
(require 'events)
(require 'spell-mode)
(require 'punctuation)
(require 'cap-signalization)


(defvar speechd-languages
  '(("en" "english")
    ("cs" "czech"))
  "Alist mapping ISO language codes to Festival language names.
Each element of the alist is of the form (LANGUAGE-CODE LANGUAGE-NAME), where
both the elements are strings.
See also `speechd-language-voices'.")

(defvar speechd-language-voices
  '(("english"
     ("male1" voice_kal_diphone)
     ("male2" voice_ked_diphone))
    ("americanenglish"
     ("male1" voice_kal_diphone)
     ("male2" voice_ked_diphone))
    ("britishenglish"
     ("male1" voice_kal_diphone)
     ("male2" voice_ked_diphone))
    ("czech"
     ("male1" voice_czech)))
  "Alist mapping Festival language names and voice names to voice functions.
Each element of the alist is of the form (LANGUAGE VOICE-LIST), where elements
of VOICE-LIST are of the form (VOICE-NAME VOICE-FUNCTION).  VOICE-NAME is a
voice name string and VOICE-FUNCTION is the name of the function setting the
given voice.")


(Parameter.set 'Wavefiletype 'nist)


(define (speechd-set-lang-voice lang voice)
  (let ((voice-func (cadr
                     (assoc_string
                      voice
                      (cdr (assoc_string lang speechd-language-voices))))))
    (if voice-func
        (apply voice-func nil)
        (error "Undefined voice"))))

(define (speechd-send-to-client result)
  (if (consp result)
      (begin
        (send_sexpr_to_client (length result))
        (mapcar utt.send.wave.client result)
        nil)
      (utt.send.wave.client result)))


;;; Commands


(define (speechd-speak* text)
  (synth-event 'text text))
(define (speechd-speak text)
  "(speechd-speak TEXT)
Speak TEXT."
  (speechd-send-to-client (speechd-speak* text)))

(define (speechd-spell* text)
  (spell_init_func)
  (unwind-protect
      (prog1 (synth-event 'text text)
        (spell_exit_func))
    (spell_exit_func)))
(define (speechd-spell text)
  "(speechd-spell TEXT)
Spell TEXT."
  (speechd-send-to-client (speechd-spell* text)))

(define (speechd-sound-icon* name)
  (synth-event 'logical name))
(define (speechd-sound-icon name)
  "(speechd-sound-icon NAME)
Play the sound or text bound to the sound icon named by the symbol NAME."
  (speechd-send-to-client (speechd-sound-icon* name)))

(define (speechd-character* character)
  (synth-event 'character character))
(define (speechd-character character)
  "(speechd-character CHARACTER)
Speak CHARACTER, represented by a string."
  (speechd-send-to-client (speechd-character* character)))

(define (speechd-key* key)
  (synth-event 'key key))
(define (speechd-key key)
  "(speechd-key KEY)
Speak KEY, represented by a string."
  (speechd-send-to-client (speechd-key* key)))

(define (speechd-set-language language)
  "(speechd-set-language language)
Set current language to LANGUAGE, where LANGUAGE is the language ISO code,
given as a two-letter string."
  (let ((lang (cadr (assoc_string language speechd-languages))))
    (if lang
        (speechd-set-lang-voice lang "male1")
        (error "Undefined language"))))

(define (speechd-set-punctuation-mode mode)
  "(speechd-set-punctuation-mode MODE)
Set punctuation mode to MODE, which is one of the symbols `all' (read all
punctuation characters), `none' (don't read any punctuation characters) or
`some' (default reading of punctuation characters)."
  (if (eq? mode 'some)
      (set! mode 'default))
  (set-punctuation-mode mode))

(define (speechd-set-voice voice)
  "(speechd-set-voice VOICE)
Set voice which must be one of the strings present in `speechd-language-voices'
alist for the current language."
  (speechd-set-lang-voice (Param.get 'Language) voice))

(define (speechd-set-rate rate)
  "(speechd-set-rate RATE)
Set speech RATE, which must be a number in the range -100..100."
  (Param.set 'Duration_Stretch
             (if (< rate 0)
                 (- 1 (/ rate 50))
                 (- 1 (/ rate 200)))))

(define (speechd-set-pitch pitch)
  "(speechd-set-pitch PITCH)
Set speech PITCH, which must be a number in the range -100..100."
  ;; TODO: Provide proper value for other intonation modules
  (set! int_lr_params
        `((target_f0_mean ,(if (< pitch 0)
                               (+ 100 (/ pitch 2))
                               (+ 100 pitch)))
          (target_f0_std 14)
          (model_f0_mean 170)
          (model_f0_std 34))))

(define (speechd-set-capital-character-recognition-mode mode)
  "(speechd-set-capital-character-recognition-mode MODE)
Enable (if MODE is non-nil) or disable (if MODE is nil) capital character
recognition mode."
  (set-cap-signalization-mode mode))

(define (speechd-list-voices)
  "(speechd-list-voices)
Return the list of the voice names (represented by strings) available for the
current language."
  (mapcar car (cdr (assoc_string (Param.get 'Language)
                                 speechd-language-voices))))
