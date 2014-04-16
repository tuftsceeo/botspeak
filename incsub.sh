SUBPATH=Devices/Thymio
git rm --cached $SUBPATH # delete reference to submodule HEAD (no trailing slash)
git rm .gitmodules             # if you have more than one submodules,
                               # you need to edit this file instead of deleting!
rm -rf $SUBPATH/.git     # make sure you have backup!!
git add $SUBPATH         # will add files instead of commit reference
git commit -m "remove submodule"
