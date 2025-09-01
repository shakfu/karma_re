1. first param reduction `reduced n_params` 7f1bc8a440dd78545dcd9bf451da4f0f1f860bdf
	works

2. second param reduction `round 2 param reduction` eeacf97aa7fe0698ece76bb221b3802a05052284
doesn't work


What changed between these two?


git diff 7f1bc8a440dd78545dcd9bf451da4f0f1f860bdf eeacf97aa7fe0698ece76bb221b3802a05052284 > changes.diff

OR

git show eeacf97aa7fe0698ece76bb221b3802a05052284 > show.diff

OR 

git diff eeacf97aa7fe0698ece76bb221b3802a05052284^! > changes2.diff