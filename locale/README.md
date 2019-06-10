# How to import symbols dictionaries

* Create a directory named symbolsrc in the locale directory of Speech-Dispatcher source tree.
* Download NVDA sources using this link: (https://github.com/nvaccess/nvda/archive/beta.zip)
* Extract the archive in the newly created directory which will create a nvda-beta folder
* Download the latest unicode CLDR release from this webpage: (http://cldr.unicode.org/index/downloads)
* In the table, chose the latest release which has a date specified and click on the "data" link
* Click on "core.zip and download it, then extract it into a sub-directory of symbolsrc named cldr
* Download UnicodeData.txt: (ftp://ftp.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt and put it into the symbolsrc directory)
* In locale, type:
```
make import-symbols
```

