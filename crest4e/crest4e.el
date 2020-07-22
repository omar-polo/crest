;;; crest4e.el --- Emacs wrapper for crest, an http repl. -*- lexical-binding: t -*-

;; Copyright (c) 2020 Omar Polo

;; Author Omar Polo <op@omarpolo.com>
;; Version: 0.1.0
;; Created: 15 July 2020
;; Keywords: http repl

;; This file is not part of GNU Emacs

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

;;; Code:

;; The mastering emacs post ``comint: writing your own command
;; interpreter'' was the base for this code.  See
;; https://masteringemacs.org/article/comint-writing-command-interpreter

(require 'comint)

(defvar crest4e-file-path (executable-find "crest")
  "Path to the executable used by `run-crest'.")

(defvar crest4e-arguments '("-i")
  "Command line arguments passed to `crest'.")

(defvar crest4e-mode-map
  (let ((map (nconc (make-sparse-keymap) comint-mode-map)))
    (define-key map "\t" 'completion-at-point)
    map)
  "Basic mode map for `run-crest'.")

(defvar crest4e-prompt-regexp "\\>"
  "Prompt for `run-crest'.")

(defun run-crest ()
  "Run an inferior instance of `crest' inside Emacs."
  (interactive)
  (let* ((buffer (comint-check-proc "crest")))
    (pop-to-buffer-same-window
     (if (or buffer (not (derived-mode-p 'crest4e-mode))
             (comint-check-proc (current-buffer)))
         (get-buffer-create (or buffer "*crest4e*"))
       (current-buffer)))

    ;; create the comint process if there is no buffer
    (unless buffer
      (apply 'make-comint-in-buffer "crest4e" buffer
             crest4e-file-path nil crest4e-arguments)
      (crest4e-mode))))

(defconst crest4e-builtins
  '("show" "set" "unset" "add" "del" "quit" "exit" "help" "usage"
    "delete" "get" "head" "options" "post"))

(defconst crest4e-keywords
  '("headers" "useragent" "prefix" "http" "http-version" "port" "peer-verification"))

(defvar crest4e-font-lock-keywords
  (list
   `(,(concat "\\_<" (regexp-opt crest4e-builtins) "\\_>") . font-lock-builtin-face)
   `(,(concat "\\_<" (regexp-opt crest4e-keywords)  "\\_>") . font-lock-keyword-face))
  "Additional expressions to highlight in `crest4e-mode'.")

(defun crest4e--initialize ()
  "Helper function to initialize crest4e."
  (setq comint-process-echoes t
        comint-use-prompt-regexp t))

(define-derived-mode crest4e-mode comint-mode "crest"
  "Major mode for `run-crest'.

\\<crest4e-mode-map>"
  nil "crest"

  (setq comint-prompt-regexp crest4e-prompt-regexp)
  (setq comint-prompt-read-only t)
  (set  (make-local-variable 'paragraph-separate) "\\'")
  (set  (make-local-variable 'font-lock-defaults) '(crest4e-font-lock-keywords t))
  (set  (make-local-variable 'paragraph-start) crest4e-prompt-regexp)
  (set  (make-local-variable 'font-lock-defaults) '(crest4e-font-lock-keywords t)))

(add-hook 'crest4e-mode-hook 'crest4e--initialize)

(provide 'crest4e)

;;; crest4e.el ends here
