(defun fib (n)
  (if (= n 1)
    1
    (if (= n 2)
      1
      (+ (fib (- n 1))
         (fib (- n 2))))))

(fib 5)
