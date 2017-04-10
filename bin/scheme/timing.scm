(provide 'timing.scm)


#!!

Create full swing bar from swing
================================

Input:
Swing - list<pos, value>
Beats - list<pos, barnum, beatnum>

Output:
Swing - list<pos, value> ;; All values are filled out.


Create swing filledout list
===========================

Input:
======

PrevSwing - list<pos, value> ;; Swing of previous track. If a value is 0, it means that automatic repeat of swing stops. (Same thing also happens if the measure changes)
                             ;; Perhaps BlockSwing is better than PrevTrackSwing? (no, because then we would need a custom "BlockSwing" track). 
                             ;; But perhaps FirstTrackSwing instead of PrevTrackSwing?
                             ;;    + If adding swing at Track #2, there won't be any swing on Track #3, unless there is swing on Track #1
                             ;;    - Track #1 will have a special meaning, but maybe that is not such a bad thing.
                             ;;    - Prev track could give some interesting results.
                             ;;    + If adding a custom BlockSwing track later, the behavior will be more similar and it might be easier to load older songs.
                             ;;    - If using previous track, it doesn't make very much sense to make a custom BlockSwing track.
                             ;;    - It might be more common that you want to use previous track.
                             ;;    + It might be more common to use a default swing track.
                             ;;    + Probably less confusing, but not sure. For sure less confusing to use custom BlockSwing track though.
                             ;;    + A BlockSwing track is more excplicit (not sure)
                             ;;    - The missing numbers are filled in automatically anyway, so it doesn't matter that much if it's less confusing.
                             ;;    + If changing swing of track 3, it's likely that you don't want to change swing of track 4.
Swing     - list<pos, value> ;; Same behavior if value is 0 as above.
Beats     - list<pos, barnum, beatnum>
LPBs      - list<pos, lpb>
DefaultLPB - integer
NumLines  - integer

Output:
list<pos, value> ;; A value can not be 0, and all automatic repeat of swing has been filled out.


!!#

::::::::::::::::::::::::::::::::::::::
;;;;;;;;;;;; BAR :::::::::::::::::::::
::::::::::::::::::::::::::::::::::::::

(define (create-beat line num)
  (list line num))

(define (get-beat-line beat)
  (car beat))

(define (get-beat-num beat)
  (cadr beat))

(define-struct bar
  :barnum
  :beats)

(define (get-first-line-in-bar bar)
  (get-beat-line 
   (car (bar :beats))))

(define (get-last-line-in-bar bar)
  (get-beat-line 
   (car (bar :beats))))




::::::::::::::::::::::::::::::::::::::
;;;;;;;;;;;; SWING :::::::::::::::::::
::::::::::::::::::::::::::::::::::::::

(define (create-swing line weight)
  (list line weight))

(define (get-swing-line swing)
  (car swing))

(define (get-swing-weight swing)
  (cadr swing))


(define-struct bar-swing
  :barnum
  :swings
  :num-lines)


;;(define (bars-have-same-signatures? prev-bar prev-bar-length bar curr-bar-length)
;;  (= prev-bar-length curr-bar-length)) ;; Probably good enough.


(define *test-shuffle-swing* '((0 1)
                               (2 2)
                               (4 1)
                               (6 2)
                               (8 1)
                               (10 2)
                               (12 1)
                               (14 2)))

(define *test-4/4-beats* (list (make-bar :barnum 0 :beats '(( 0 0)
                                                            ( 4 1)
                                                            ( 8 2)
                                                            (12 3)))
                               (make-bar :barnum 1 :beats '((16 0)
                                                            (20 1)
                                                            (24 2)
                                                            (28 3)))))


#||
;; Currently just moves the first swing in the bar to the start of the bar. (not needed, incorportated into 'group-swing-by-bar' instead)
(define (add-bar-start-to-swing bars swing)
  (if (null? bars)
      '()
      (let* ((bar (car bars))
             (next-bar (cl-cadr bars))
             (first-line (car bar))
             (next-line (or (and next-bar (car (cadr next-bar)))
                            num-lines)))

        )))

(***assert*** (add-bar-start-swings '((0 (0 0))
                                      (1 (2 0)))
                                    '((0 1)
                                      (1 2)
                                      (3 5)))
              '((0 1)
                (1 2)
                (2 5)))
(***assert*** (add-bar-start-swings '((0 (0 0))
                                      (1 (2 0)))
                                    '((0 1)
                                      (1 2)
                                      (3 5)
                                      (7/2 6)))
              '((0 1)
                (1 2)
                (2 5)
                (7/2 6)))
||#           

(define (group-swing-by-bar bars swings num-lines)
  (if (null? bars)
      '()
      (let* ((bar (car bars))
             (next-bar (begin (c-display ":bar" bar ":bars" bars) (cl-cadr bars)))
             (barnum (bar :barnum))
             (first-line (get-first-line-in-bar bar))
             (next-line (if next-bar
                            (get-first-line-in-bar next-bar)
                            num-lines)))
        (let loop ((swings swings)
                   (result '())
                   (is-first-bar #t))
          (define (gotit)
            ;;(c-display "gotit" first-line result)
            (if (null? result)
                (group-swing-by-bar (cdr bars) swings num-lines)
                (cons (make-bar-swing :barnum barnum
                                      :swings (map (lambda (swing)
                                                     (create-swing (- (get-swing-line swing)
                                                                      first-line)
                                                                   (get-swing-weight swing)))
                                                   (reverse result))
                                      :num-lines (- next-line first-line))
                      (group-swing-by-bar (cdr bars) swings num-lines))))
          ;;(c-display "swing" swing)
          (c-display ":bar" bar (cl-cadr next-bar) ":first-line" first-line ":swings" swings ":result" result ":next-line" next-line)
          (if (null? swings)
              (gotit)
              (let* ((swing (car swings))
                     (swing-line (get-swing-line swing)))
                (if (>= swing-line next-line)
                    (gotit)
                    (loop (cdr swings)
                          (cons (if (and is-first-bar
                                         (> swing-line first-line))
                                    (create-swing first-line
                                                  (get-swing-weight swing))
                                    swing)
                                result)
                          #f))))))))
        
(pretty-print (group-swing-by-bar (list (make-bar :barnum 0 :beats (list (create-beat 0 0)))
                                        (make-bar :barnum 1 :beats (list (create-beat 2 0))))
                                  (list (create-swing 0 1)
                                        (create-swing 1 2)
                                        (create-swing 3 5))
                                  4))
        

(***assert*** (group-swing-by-bar (list (make-bar :barnum 0 :beats (list (create-beat 0 0)))
                                        (make-bar :barnum 1 :beats (list (create-beat 2 0))))
                                  (list (create-swing 0 1)
                                        (create-swing 1 2)
                                        (create-swing 2 4)
                                        (create-swing 3 5))
                                  4)
              (list (make-bar-swing :barnum 0 :swings (list (create-swing 0 1)
                                                            (create-swing 1 2))
                                    :num-lines 2)
                    (make-bar-swing :barnum 1 :swings (list (create-swing 0 4)
                                                            (create-swing 1 5))
                                    :num-lines 2)))


(***assert*** (group-swing-by-bar (list (make-bar :barnum 0 :beats (list (create-beat 0 0)))
                                        (make-bar :barnum 1 :beats (list (create-beat 2 0))))
                                  (list (create-swing 0 1)
                                        (create-swing 1 2)
                                        (create-swing 3 5))
                                  4)
              (list (make-bar-swing :barnum 0 :swings (list (create-swing 0 1)
                                                            (create-swing 1 2))
                                    :num-lines 2)
                    (make-bar-swing :barnum 1 :swings (list (create-swing 0 5))
                                    :num-lines 2)))


#||
(***assert*** (group-swing-by-bar *test-4/4-beats*
                                  *test-shuffle-swing*
                                  64)
              `((0 ,*test-shuffle-swing*)))
||#


;; Add autoinserted swings.
(define (create-filledout-swings bars global-swings track-swings num-lines)  
  (let loop ((bars bars)
             (prev-bar #f)
             (curr-swing #f)
             (global-swings global-swings) ;;(group-swing-by-bar bars global-swing num-lines))
             (track-swings track-swings)) ;;(group-swing-by-bar bars track-swing num-lines)))
    (cond ((null? bars)
           (assert (null? global-swings))
           (assert (null? track-swings))
           '())

          ((and (not curr-swing)
                (null? global-swings)
                (null? track-swings))
           '())

          (else

           (define bar (car bars))
           (define barnum (bar :barnum))

           (while (and (not (null? global-swings))
                       (< ((car global-swings) :barnum) barnum))
             (set! global-swings (cdr global-swings)))

           (define global-swing (cl-car global-swings))
           (define track-swing (cl-car track-swings))

           (cond ((and track-swing
                       (= barnum (track-swing :barnum)))
                  (cons track-swing
                        (loop (cdr bars)
                              bar
                              track-swing
                              global-swings
                              (cdr track-swings))))

                 ((and global-swing
                       (= barnum (global-swing :barnum)))
                  (cons global-swing
                        (loop (cdr bars)
                              bar
                              global-swing
                              (cdr global-swings)
                              track-swings)))

                 ((and curr-swing
                       (let* ((curr-line (get-first-line-in-bar bar))
                              (next-bar (cl-cadr bars))
                              (next-line (if next-bar
                                             (get-first-line-in-bar next-bar)
                                             num-lines))
                              (curr-bar-length (- next-line
                                                  curr-line)))
                         (= (curr-swing :num-lines)
                            curr-bar-length)))

                  (cons (copy-bar-swing curr-swing :barnum barnum)
                        (loop (cdr bars)
                              bar
                              curr-swing
                              global-swings
                              track-swings)))

                 (else
                  (loop (cdr bars)
                        bar
                        #f
                        global-swings
                        track-swings)))))))



(***assert*** (create-filledout-swings (list (make-bar :barnum 0 :beats (list (create-beat 0 0)))
                                             (make-bar :barnum 1 :beats (list (create-beat 2 0)))
                                             (make-bar :barnum 2 :beats (list (create-beat 4 0))))
                                       (list (make-bar-swing :barnum 0 :swings (list (create-swing 0 1)
                                                                                     (create-swing 1 2))
                                                             :num-lines 2))
                                       (list (make-bar-swing :barnum 2 :swings (list (create-swing 0 2)
                                                                                     (create-swing 1 1))
                                                             :num-lines 2))
                                       6)
              (list (make-bar-swing :barnum 0 :swings (list (create-swing 0 1)
                                                            (create-swing 1 2))
                                    :num-lines 2)
                    (make-bar-swing :barnum 1 :swings (list (create-swing 0 1)
                                                            (create-swing 1 2))
                                    :num-lines 2)
                    (make-bar-swing :barnum 2 :swings (list (create-swing 0 2)
                                                            (create-swing 1 1))
                                    :num-lines 2)))

;; Test that we don't repeat swing when there is a signature change. (actually, when the bar changes number of lines)
(***assert*** (create-filledout-swings (list (make-bar :barnum 0 :beats (list (create-beat 0 0)))
                                             (make-bar :barnum 1 :beats (list (create-beat 2 0)))
                                             (make-bar :barnum 2 :beats (list (create-beat 4 0))))
                                       (list (make-bar-swing :barnum 0 :swings (list (create-swing 0 1)
                                                                                     (create-swing 1 2))
                                                             :num-lines 2))
                                       (list (make-bar-swing :barnum 1 :swings (list (create-swing 0 2)
                                                                                     (create-swing 1 1))
                                                             :num-lines 2))
                                       5)
              (list (make-bar-swing :barnum 0 :swings (list (create-swing 0 1)
                                                            (create-swing 1 2))
                                    :num-lines 2)
                    (make-bar-swing :barnum 1 :swings (list (create-swing 0 2)
                                                            (create-swing 1 1))
                                    :num-lines 2)))


#||

Create swing multiplication list
================================

Input:
=======
FilledoutSwing - list<pos, value>
Beats - list<pos, barnum, beatnum>


Output:
list<pos,tempo_multiplication,hold>

||#


(define-struct tempo-multiplier
  :y1 :x1
  :y2 :x2)


(define (create-tempo-multipliers-from-swing start-line bar-swing)
  (define swings (bar-swing :swings))
  (define num-lines (bar-swing :num-lines))
  (define end-line (+ start-line num-lines))
  (define weight-sum (let loop ((swings swings))
                       (if (null? swings)
                           0
                           (let* ((swing (car swings))
                                  (line1 (get-swing-line swing))
                                  (next-swing (cl-cadr swings))
                                  (line2 (if next-swing
                                             (get-swing-line next-swing)
                                             num-lines))
                                  (duration (- line2 line1)))
                             (+ (* duration 
                                   (/ 1 (get-swing-weight swing)))
                                (loop (cdr swings)))))))
  (let loop ((swings swings))
    (if (null? swings)
        '()
        (let* ((swing (car swings))
               (line1 (get-swing-line swing))
               (next-swing (cl-cadr swings))
               (line2 (if next-swing
                          (get-swing-line next-swing)
                          num-lines))
               (duration (- line2 line1)))
          (define multiplier (/ (* duration
                                   (/ 1 (get-swing-weight swing))
                                   num-lines)
                                weight-sum))
          (cons (make-tempo-multiplier :y1 (+ start-line line1) :x1 multiplier
                                       :y2 (+ start-line line2) :x2 multiplier)
                (loop (cdr swings)))))))

(***assert*** (create-tempo-multipliers-from-swing 10 (make-bar-swing :barnum 1 :swings (list (create-swing 0 1)
                                                                                              (create-swing 1 2))
                                                                      :num-lines 2))
              (list (make-tempo-multiplier :y1 10 :x1 4/3
                                           :y2 11 :x2 4/3)
                    (make-tempo-multiplier :y1 11 :x1 2/3
                                           :y2 12 :x2 2/3)))
              
(***assert*** (create-tempo-multipliers-from-swing 10 (make-bar-swing :barnum 1 :swings (list (create-swing 0 1)
                                                                                              (create-swing 1 2))
                                                                      :num-lines 3))
              (list (make-tempo-multiplier :y1 10 :x1 3/2
                                           :y2 11 :x2 3/2)
                    (make-tempo-multiplier :y1 11 :x1 6/4
                                           :y2 13 :x2 6/4)))
              

#!!
(pp (create-tempo-multipliers-from-swing 10 (make-bar-swing :barnum 1 :swings (list (create-swing 0 1)
                                                                                    (create-swing 1 2))
                                                            :num-lines 3)))
(pp (create-tempo-multipliers-from-swing 10 (make-bar-swing :barnum 1 :swings (list (create-swing 0 1)
                                                                                    (create-swing 1 2)
                                                                                    (create-swing 2 2))
                                                            :num-lines 3)))
!!#

;; TODO:
;; * Call create-filled-out-bar-swings
;; * Add (:x1 1 :x1 1) for bars that has no defined bar-swing
(define (create-tempo-multipliers-from-swings bars bar-swings num-lines)
  (define bar (cl-car bars))
  (define bar-swing (cl-car bar-swings))
  (define line (and bar (get-first-line-in-bar bar)))

  (if (not bar-swing)      
      (if bar
          (list (make-tempo-multiplier :y1 line :x1 1
                                       :y2 num-lines :x2 1))
          '())
      (append (create-tempo-multipliers-from-swing line bar-swing)
              (create-tempo-multipliers-from-swings (cdr bars)
                                                    (cdr bar-swings)
                                                    num-lines))))

#!!
(pp (create-tempo-multipliers-from-swings (list (make-bar :barnum 0 :beats (list (create-beat 0 0)))
                                                (make-bar :barnum 1 :beats (list (create-beat 2 0)))
                                                (make-bar :barnum 2 :beats (list (create-beat 4 0))))
                                          (list (make-bar-swing :barnum 0 :swings (list (create-swing 0 1)
                                                                                        (create-swing 1 2))
                                                                :num-lines 2)
                                                (make-bar-swing :barnum 1 :swings (list (create-swing 0 1)
                                                                                        (create-swing 1 2))
                                                                :num-lines 2)
                                                (make-bar-swing :barnum 2 :swings (list (create-swing 0 2)
                                                                                        (create-swing 1 1))
                                                                :num-lines 2))
                                          64))
!!#

#!!
(***assert*** (create-tempo-multipliers-from-swings (list (make-bar :barnum 0 :beats (list (create-beat 0 0)))
                                                          (make-bar :barnum 1 :beats (list (create-beat 2 0)))
                                                          (make-bar :barnum 2 :beats (list (create-beat 4 0))))
                                                    (list (make-bar-swing :barnum 0 :swings (list (create-swing 0 1)
                                                                                                  (create-swing 1 2))
                                                                          :num-lines 2)
                                                          (make-bar-swing :barnum 1 :swings (list (create-swing 0 1)
                                                                                                  (create-swing 1 2))
                                                                          :num-lines 2)
                                                          (make-bar-swing :barnum 2 :swings (list (create-swing 0 2)
                                                                                                  (create-swing 1 1))
                                                                          :num-lines 2)))
              (list (make-tempo-multiplier :y1 0 :multiplier 4/3 :multiplier-end 4/3)
                    (make-tempo-multiplier :y1 1 :multiplier 2/3 :multiplier-end 2/3)
                    (make-tempo-multiplier :y1 2 :multiplier 4/3 :multiplier-end 4/3)
                    (make-tempo-multiplier :y1 3 :multiplier 2/3 :multiplier-end 2/3)
                    (make-tempo-multiplier :y1 4 :multiplier 2/3 :multiplier-end 2/3)
                    (make-tempo-multiplier :y1 5 :multiplier 4/3 :multiplier-end 4/3)))
!!#



;;;;;;;;;;;;;;;;;;;;;;;


(define (make-tempo-multipliers-cover-all-lines as max-y)
  (if (null? as)
      (list (make-tempo-multiplier :y1 0 :x1 1
                                   :y2 max-y :x2 1))
      (let ((as (let ((y1 ((car as) :y1)))
                  (if (> y1 0)
                      (cons (make-tempo-multiplier :y1 0 :x1 1
                                                   :y2 y1 :x2 1)
                            as)
                      as))))
        (let* ((l (last as))
               (y2 (l :y2)))
          (assert (<= y2 max-y))
          (if (= y2 max-y)
              as
              (append as (list (make-tempo-multiplier :y1 y2    :x1 1
                                                      :y2 max-y :x2 1))))))))

(define (merge-tempo-multipliers as bs max-y)
  (let loop ((as (make-tempo-multipliers-cover-all-lines as max-y))
             (bs (make-tempo-multipliers-cover-all-lines bs max-y))
             (y1 0))
             
    (if (= y1 max-y)
        '()
        (let* ((a (car as))
               (b (car bs))
               (a_y2 (a :y2))
               (b_y2 (b :y2)))
          
          (cond ((<= a_y2 y1)
                 (loop (cdr as)
                       bs
                       y1))

                ((<= b_y2 y1)
                 (loop as
                       (cdr bs)
                       y1))

                (else
                 (define (get-x y)
                   (* (scale y
                             (a :y1) (a :y2)
                             (a :x1) (a :x2))
                      (scale y
                             (b :y1) (b :y2)
                             (b :x1) (b :x2))))
                 (define y2 (min a_y2 b_y2))
                 (cons (make-tempo-multiplier :y1 y1 :x1 (get-x y1)
                                              :y2 y2 :x2 (get-x y2))
                       (loop as bs y2))))))))

;; test 1: same start and stop
(***assert*** (merge-tempo-multipliers (list (make-tempo-multiplier :y1 0 :x1 3
                                                                    :y2 4 :x2 4))
                                       (list (make-tempo-multiplier :y1 0 :x1 5
                                                                    :y2 4 :x2 6))
                                       4)
              (list (make-tempo-multiplier :y1 0 :x1 (* 3 5)
                                           :y2 4 :x2 (* 4 6))))

;; test 2: same start and stop, but does not cover all lines
(***assert*** (merge-tempo-multipliers (list (make-tempo-multiplier :y1 0 :x1 3
                                                                    :y2 4 :x2 4))
                                       (list (make-tempo-multiplier :y1 0 :x1 5
                                                                    :y2 4 :x2 6))
                                       8)
              (list (make-tempo-multiplier :y1 0 :x1 (* 3 5)
                                           :y2 4 :x2 (* 4 6))
                    (make-tempo-multiplier :y1 4 :x1 1
                                           :y2 8 :x2 1)))

;; test 3: a starts later than b
(***assert*** (merge-tempo-multipliers (list (make-tempo-multiplier :y1 1 :x1 3
                                                                    :y2 4 :x2 4))
                                       (list (make-tempo-multiplier :y1 0 :x1 5
                                                                    :y2 4 :x2 6))
                                       8)
              (list (make-tempo-multiplier :y1 0 :x1 5
                                           :y2 1 :x2 (scale 1 0 4 5 6))
                    (make-tempo-multiplier :y1 1 :x1 (* 3 (scale 1 0 4 5 6))
                                           :y2 4 :x2 (* 4 6))
                    (make-tempo-multiplier :y1 4 :x1 1
                                           :y2 8 :x2 1)))

;; test 4: b stops later than a
(***assert*** (merge-tempo-multipliers (list (make-tempo-multiplier :y1 0 :x1 3
                                                                    :y2 4 :x2 4))
                                       (list (make-tempo-multiplier :y1 0 :x1 5
                                                                    :y2 5 :x2 6))
                                       8)
              (list (make-tempo-multiplier :y1 0 :x1 (* 3 5)
                                           :y2 4 :x2 (* 4 (scale 4 0 5 5 6)))
                    (make-tempo-multiplier :y1 4 :x1 (scale 4 0 5 5 6)
                                           :y2 5 :x2 6)
                    (make-tempo-multiplier :y1 5 :x1 1
                                           :y2 8 :x2 1)))

;; test 5: a starts and stops before b, but they overlap
(***assert*** (merge-tempo-multipliers (list (make-tempo-multiplier :y1 0 :x1 3
                                                                    :y2 4 :x2 4))
                                       (list (make-tempo-multiplier :y1 1 :x1 5
                                                                    :y2 5 :x2 6))
                                       8)
              (list (make-tempo-multiplier :y1 0 :x1 3
                                           :y2 1 :x2 (scale 1 0 4 3 4))
                    (make-tempo-multiplier :y1 1 :x1 (* 5 (scale 1 0 4 3 4))
                                           :y2 4 :x2 (* 4 (scale 4 1 5 5 6)))
                    (make-tempo-multiplier :y1 4 :x1 (scale 4 1 5 5 6)
                                           :y2 5 :x2 6)
                    (make-tempo-multiplier :y1 5 :x1 1
                                           :y2 8 :x2 1)))


;; test 6: a starts and stops before b, and they don't overlap
(***assert*** (merge-tempo-multipliers (list (make-tempo-multiplier :y1 0 :x1 3
                                                                    :y2 3 :x2 4))
                                       (list (make-tempo-multiplier :y1 4 :x1 5
                                                                    :y2 5 :x2 6))
                                       8)
              (list (make-tempo-multiplier :y1 0 :x1 3
                                           :y2 3 :x2 4)
                    (make-tempo-multiplier :y1 3 :x1 1
                                           :y2 4 :x2 1)                    
                    (make-tempo-multiplier :y1 4 :x1 (* 1 5)
                                           :y2 5 :x2 (* 1 6))
                    (make-tempo-multiplier :y1 5 :x1 1
                                           :y2 8 :x2 1)))


;; test 7: a starts before b, and b stops before a
(***assert*** (merge-tempo-multipliers (list (make-tempo-multiplier :y1 0 :x1 3
                                                                    :y2 5 :x2 4))
                                       (list (make-tempo-multiplier :y1 1 :x1 5
                                                                    :y2 4 :x2 6))
                                       8)
              (list (make-tempo-multiplier :y1 0 :x1 3
                                           :y2 1 :x2 (* 1 (scale 1 0 5 3 4)))
                    (make-tempo-multiplier :y1 1 :x1 (* 5 (scale 1 0 5 3 4))
                                           :y2 4 :x2 (* 6 (scale 4 0 5 3 4)))
                    (make-tempo-multiplier :y1 4 :x1 (* 1 (scale 4 0 5 3 4))
                                           :y2 5 :x2 (* 1 (scale 5 0 5 3 4)))
                    (make-tempo-multiplier :y1 5 :x1 1
                                           :y2 8 :x2 1)))




;;;;;;;;;;;;;;;;;;;;;;;

#||

(define (get-duration num-lines samplerate lpb bpm tempo-multiplier)
  (/ (* num-lines 60 samplerate)
     (* lpb bpm tempo-multiplier)))

(get-duration 64 48000 4 120 1)
(* 48000 8)
||#

                                                         

(define (bpms-or-lpbs-to-tempo-multipliers main-bpm bpms num-lines value-key)
  (define (get-tempo-multiplier bpm)
    (/ bpm
       main-bpm))
  (let loop ((bpms bpms))
    (if (null? bpms)
        '()
        (let* ((bpm1 (car bpms))
               (bpm2 (cl-cadr bpms))
               (x1 (get-tempo-multiplier (bpm1 value-key)))
               (x2 (if (or (not bpm2)
                           (= (bpm1 :logtype) *logtype-hold*))
                       x1
                       (get-tempo-multiplier (bpm2 value-key)))))
          (cons (make-tempo-multiplier :y1 (bpm1 :place)
                                       :x1 x1
                                       :y2 (if bpm2
                                               (bpm2 :place)
                                               num-lines)
                                       :x2 x2)
                (loop (cdr bpms)))))))
                                               
(define (bpms-to-tempo-multipliers main-bpm bpms num-lines)
  (bpms-or-lpbs-to-tempo-multipliers main-bpm bpms num-lines :bpm))

(define (lpbs-to-tempo-multipliers main-lpb lpbs num-lines)
  (bpms-or-lpbs-to-tempo-multipliers main-lpb lpbs num-lines :lpb))


(***assert*** (bpms-to-tempo-multipliers 120
                                         (list (hash-table* :bpm 20
                                                            :place 1
                                                            :logtype *logtype-hold*))
                                         64)
              (list (make-tempo-multiplier :y1  1 :x1 (/ 20 120)
                                           :y2  64 :x2 (/ 20 120))))

(***assert*** (lpbs-to-tempo-multipliers 4
                                         (list (hash-table* :lpb 20
                                                            :place 1
                                                            :logtype *logtype-hold*))
                                         64)
              (list (make-tempo-multiplier :y1  1 :x1 (/ 20 4)
                                           :y2  64 :x2 (/ 20 4))))

(***assert*** (bpms-to-tempo-multipliers 120
                                         (list (hash-table* :bpm 20
                                                            :place 1
                                                            :logtype *logtype-hold*)
                                               (hash-table* :bpm 50
                                                            :place 8
                                                            :logtype *logtype-hold*))
                                         64)
              (list (make-tempo-multiplier :y1  1 :x1 (/ 20 120)
                                           :y2  8 :x2 (/ 20 120))
                    (make-tempo-multiplier :y1  8 :x1 (/ 50 120)
                                           :y2 64 :x2 (/ 50 120))))

(***assert*** (bpms-to-tempo-multipliers 120
                                         (list (hash-table* :bpm 200
                                                            :place 1
                                                            :logtype *logtype-linear*)
                                               (hash-table* :bpm 50
                                                            :place 8
                                                            :logtype *logtype-linear*)) ;; Logtype of last bpm is ignored.
                                         64)
              (list (make-tempo-multiplier :y1  1 :x1 (/ 200 120)
                                           :y2  8 :x2 (/ 50 120))
                    (make-tempo-multiplier :y1  8 :x1 (/ 50 120)
                                           :y2 64 :x2 (/ 50 120))))


;;(pp (<ra> :get-all-bpm))


(define (temponodes-to-tempo-multipliers temponodes)
  (let* ((node1 (car temponodes))
         (node2 (cadr temponodes))
         (x1 (node1 :tempo-multiplier))
         (x2 (if (= (node1 :logtype) *logtype-hold*)
                 x1
                 (node2 :tempo-multiplier))))
    (cons (make-tempo-multiplier :y1 (node1 :place)
                                 :x1 x1
                                 :y2 (node2 :place)
                                 :x2 x2)
          (if (null? (cddr temponodes))
              '()
              (temponodes-to-tempo-multipliers (cdr temponodes))))))

(***assert*** (temponodes-to-tempo-multipliers (list (hash-table* :place 0
                                                                  :tempo-multiplier 1
                                                                  :logtype *logtype-linear*)
                                                     (hash-table* :place 64
                                                                  :tempo-multiplier 1
                                                                  :logtype *logtype-linear*)))
              (list (make-tempo-multiplier :y1 0 :x1 1
                                           :y2 64 :x2 1)))

(***assert*** (temponodes-to-tempo-multipliers (list (hash-table* :place 0
                                                                  :tempo-multiplier 0.5
                                                                  :logtype *logtype-linear*)
                                                     (hash-table* :place 1
                                                                  :tempo-multiplier 0.1
                                                                  :logtype *logtype-linear*)
                                                     (hash-table* :place 64
                                                                  :tempo-multiplier 1.2
                                                                  :logtype *logtype-linear*)))
              (list (make-tempo-multiplier :y1 0 :x1 0.5
                                           :y2 1 :x2 0.1)
                    (make-tempo-multiplier :y1 1 :x1 0.1
                                           :y2 64 :x2 1.2)))

#||
(pp (<ra> :get-all-temponodes))
(<ra> :get-temponode-place 3)
||#


#!!

(define (place->stime-0 y
                        y1 y2
                        x1 x2)

  (if (= x1 x2)
      (/ y x1)
      (begin
        (define k (/ (- x2 x1)
                     (- y2 y1)))
        (define bp (scale y y1 y2 x1 x2))
        (define T1 x1)
        
        (define Tbp (scale y y1 y2 x1 x2))
        
        (define dur (* (/ 1 k)
                       (log (/ Tbp
                               T1))))
        
        (c-display "y" y "dur" dur "k" k ", bp" bp ", T1" T1 ", Tbp" Tbp)
        
        dur)))

(log (/ 0.125 0.5))

(place->stime-0 0.1
                0 2
                0.5 1)
(/ (- 10 5)
   (- 5 10))

(define g1 (draw-plot (map (lambda (y) (/ y 10))
                           (iota 100))
                      (lambda (y)
                        (place->stime-0 y
                                        0 10
                                        5.0 5.2))))
(define g1 (draw-plot (map (lambda (y) (/ y 10))
                           (iota 100))
                      (lambda (y)
                        (place->stime-0 y
                                        0 10
                                        5.0 5.1))))
(define g1 (draw-plot (map (lambda (y) (/ y 10))
                           (iota 100))
                      (lambda (y)
                        (place->stime-0 y
                                        0 10
                                        5.0 5.0))))
(define g1 (draw-plot (map (lambda (y) (/ y 10))
                           (iota 100))
                      (lambda (y)
                        (place->stime-0 y
                                        0 64
                                        (/ 50 60) (/ 200 60)))))


(define g2 (draw-plot (map (lambda (y) (/ y 10))
                           (iota 100))
                      (lambda (y)
                        (place->stime-0 y
                                        0 10
                                        5.2 0.1))))
!!#

(define (create-block-timings2 global-bpm global-lpb tempo-multipliers num-lines)
  (define x (* global-bpm global-lpb))
  (define g-tempos (list (make-tempo-multiplier :y1 0 :x1 x
                                                :y2 num-lines :x2 x)))
  (c-display "__MERGED" (merge-tempo-multipliers g-tempos tempo-multipliers num-lines))
  (merge-tempo-multipliers g-tempos tempo-multipliers num-lines))
                 

#||
(create-block-timings2 120
                       4
                       (list (make-tempo-multiplier :y1 0 :x1 1.0
                                                    :y2 64 :x2 1.0))
                       64
                       48000)

(***assert*** (create-block-timings2 120
                                     4
                                     (list (make-tempo-multiplier :y1 0.0 :x1 1.0
                                                                  :y2 0.5 :x2 1.0)
                                           (make-tempo-multiplier :y1 0.5 :x1 1.0
                                                                  :y2 1.0 :x2 1.0))
                                     
                                     1
                                     48000)
              (list (make-time-change :y1 0.0 :x1 1.0 :t1 0
                                      :y2 0.5 :x2 1.0 :t2 24000)
                    (make-time-change :y1 0.5 :x1 1.0 :t1 24000
                                      :y2 1.0 :x2 1.0 :t2 48000)))

(***assert*** (create-block-timings2 120
                                     4
                                     (list (make-tempo-multiplier :y1 0 :x1 1.0
                                                                  :y2 1 :x2 1.0))
                                     1
                                     48000)
              (list (list 0)
                    (list 48000)))

(create-block-timings2 120
                       4
                       (list (make-tempo-multiplier :y1 0 :x1 1.0
                                                    :y2 1 :x2 2.0))
                       1
                       48000)

(***assert*** (create-block-timings2 (list (make-tempo-multiplier :y1 0 :x1 1.0
                                                                 :y2 1 :x2 2.0))
                                    1
                                    48000)
              (list (list 0)
                    (list (* 48000 (place->stime0 1
                                                  0 1
                                                  1 2)))))


||#


(define (create-block-timings num-lines main-bpm main-lpb bpms lpbs temponodes)
  (define tempo-multipliers (merge-tempo-multipliers (merge-tempo-multipliers (bpms-to-tempo-multipliers main-bpm (vector->list bpms) num-lines)
                                                                              (lpbs-to-tempo-multipliers main-lpb (vector->list lpbs) num-lines)
                                                                              num-lines)
                                                     (temponodes-to-tempo-multipliers (vector->list temponodes))
                                                     num-lines))

  (create-block-timings2 main-bpm main-lpb tempo-multipliers num-lines))


(define (create-block-timings-for-block blocknum)
  (create-block-timings (<ra> :get-num-lines blocknum)
                        (<ra> :get-main-bpm)
                        (<ra> :get-main-lpb)
                        (<ra> :get-all-bpm blocknum)
                        (<ra> :get-all-lpb blocknum)
                        (<ra> :get-all-temponodes blocknum)))


#||

(when *is-initializing*
  (***assert*** (length (create-block-timings -1))
                64)
  (***assert*** (first (create-block-timings -1))
                0)
  (***assert*** (/ (last (create-block-timings -1))
                   (<ra> :get-sample-rate))
                8))


||#



#||

Input:
=======

DefaultLPB - integer
DefaultBPM - integer
LPB - list<pos,value,is_hold>
BPM - list<pos,value,is_hold>
TempoAutomation - list<pos,tempoMul>, with at least two nodes. The first node has position 0, the last node has position NumLines.
Swing - list<?>
<strike>NumLines - integer</strike>
<strike>Samplerate - integer</strike>


Output:
=======

<strike>Timing - list, with at least one node per integer. First node has the value 0. Length is NumLines + 1.</strike>
Tempo automation: list<pos,tempo multiplication, is_hold>


Swing:
=======

0 -> 1
1 -> 2
2 -> 1
3 -> 2
4 -> 1
5 -> 2
6 -> 1
 ....
 ===>
0 -> 1.33
1 -> 0.66
2 -> 1.33
3 -> 0.66
 ....


LPB:
======
0 -> 1
1 -> 2
2 -> 1
3 -> 2
4 -> 1
5 -> 2
6 -> 1
 ....
 ===>
0 -> 1   (1/1
1 -> 0.5 (1/2)
2 -> 1   (1/2)
3 -> 0.5 (1/1
 ....


BPM:
======
0 -> 100
1 -> 200
2 -> 100
3 -> 200
4 -> 100
5 -> 200
6 -> 100
 ....
 ===>
0 -> 60/100
1 -> 60/200
2 -> 60/100
3 -> 60/200
 ....



(define (get-duration duration-in-lines samplerate lpb bpm tempo-multiplier)
  (/ (* duration-in-lines 60 samplerate)
     (* lpb bpm tempo-multiplier)))
  

(***assert*** (/ (get-duration 64 48000 4 120 1)
                 48000)
              8)

||#


