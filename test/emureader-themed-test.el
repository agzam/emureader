;;; emureader-themed-test.el --- Tests for emureader-themed-mode -*- lexical-binding: t; -*-

;;; Commentary:

;; Run from the repository root:
;;   `emacs -Q --batch --init-directory="$(mktemp -d)" -L .
;;    -l test/emureader-themed-test.el -f ert-run-tests-batch-and-exit'
;; No dynamic module is loaded; its tint setter is stubbed.

;;; Code:

(require 'ert)
(require 'cl-lib)
(require 'emureader)

(defvar emureader-test--colors '("#102030" . "#f0e0d0")
  "The foreground and background the stubbed `default' face reports.")

(defvar emureader-test--tints nil
  "Arguments of each stubbed `emureader-dyn--doc-tint' call, newest first.")

(defmacro emureader-test--stubbed (&rest body)
  "Run BODY with the module, the redisplay and the `default' face colors stubbed.
A batch frame rounds `color-values' to its few terminal colors, so the
hex specs are parsed exactly instead."
  `(let ((emureader-test--tints nil)
         (enable-theme-functions nil)
         (disable-theme-functions nil))
     (cl-letf (((symbol-function 'emureader-dyn--doc-tint)
                (lambda (black white) (push (list black white) emureader-test--tints)))
               ((symbol-function 'emureader--redisplay-all-windows) #'ignore)
               ((symbol-function 'color-values) #'color-values-from-color-spec)
               ((symbol-function 'face-foreground)
                (lambda (&rest _) (car emureader-test--colors)))
               ((symbol-function 'face-background)
                (lambda (&rest _) (cdr emureader-test--colors))))
       ,@body)))

(defun emureader-test--document (&optional loaded)
  "Make a buffer that passes for an `emureader-mode' document.
LOADED gives it a DocState."
  (let ((buffer (generate-new-buffer " emureader-test")))
    (with-current-buffer buffer
      (setq major-mode 'emureader-mode)
      (when loaded
        (setq-local emureader-current-doc-state-ptr t)))
    buffer))

(ert-deftest emureader-themed-mode-tints-in-default-face-colors ()
  (emureader-test--stubbed
   (let ((doc (emureader-test--document t)))
     (unwind-protect
         (with-current-buffer doc
           (emureader-themed-mode 1)
           (should (equal emureader-tint-colors '(#x102030 . #xf0e0d0)))
           (emureader-themed-mode -1)
           (should-not emureader-tint-colors)
           (should (equal emureader-test--tints
                          '((nil nil) (#x102030 #xf0e0d0)))))
       (kill-buffer doc)))))

(ert-deftest emureader-themed-mode-leaves-an-unloaded-document-to-the-load ()
  (emureader-test--stubbed
   (let ((doc (emureader-test--document)))
     (unwind-protect
         (with-current-buffer doc
           (emureader-themed-mode 1)
           (should (equal emureader-tint-colors '(#x102030 . #xf0e0d0)))
           (should-not emureader-test--tints))
       (kill-buffer doc)))))

(ert-deftest emureader-themed-mode-follows-theme-changes ()
  (emureader-test--stubbed
   (let ((themed (emureader-test--document t))
         (plain (emureader-test--document t))
         (other (generate-new-buffer " emureader-test-other")))
     (unwind-protect
         (progn
           (with-current-buffer themed (emureader-themed-mode 1))
           (with-current-buffer other (setq-local emureader-themed-mode t))
           (should (memq #'emureader--refresh-themed-buffers enable-theme-functions))
           (should (memq #'emureader--refresh-themed-buffers disable-theme-functions))
           (setq emureader-test--tints nil)
           (let ((emureader-test--colors '("#ffffff" . "#000000")))
             (run-hook-with-args 'enable-theme-functions 'some-theme))
           (should (equal (buffer-local-value 'emureader-tint-colors themed)
                          '(#xffffff . #x000000)))
           (should-not (buffer-local-value 'emureader-tint-colors plain))
           (should (equal emureader-test--tints '((#xffffff #x000000))))
           (let ((emureader-test--colors '("#000000" . "#ffffff")))
             (run-hook-with-args 'disable-theme-functions 'some-theme))
           (should (equal (buffer-local-value 'emureader-tint-colors themed)
                          '(#x000000 . #xffffff))))
       (mapc #'kill-buffer (list themed plain other))))))

(ert-deftest emureader-global-themed-mode-themes-documents-only ()
  (emureader-test--stubbed
   (let ((doc (emureader-test--document))
         (other (generate-new-buffer " emureader-test-other")))
     (unwind-protect
         (progn
           (emureader-global-themed-mode 1)
           (should (buffer-local-value 'emureader-themed-mode doc))
           (should-not (buffer-local-value 'emureader-themed-mode other))
           (with-temp-buffer
             (setq major-mode 'emureader-mode)
             (run-hooks 'after-change-major-mode-hook)
             (should emureader-themed-mode))
           (emureader-global-themed-mode -1)
           (should-not (buffer-local-value 'emureader-themed-mode doc)))
       (emureader-global-themed-mode -1)
       (mapc #'kill-buffer (list doc other))))))

(provide 'emureader-themed-test)
;;; emureader-themed-test.el ends here
