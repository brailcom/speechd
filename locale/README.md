# How to import symbols dictionaries

* Get gettext version 0.20 or later
* Download NVDA sources using this link: https://github.com/nvaccess/nvda/archive/beta.zip
* Extract the archive in the locale/symbolsrc sub-directory of your Speech-Dispatcher repository, which will create a nvda-beta folder
* Download the latest unicode CLDR release by clicking the `"cldr-common-<version>.zip"` link on this web page: https://unicode.org/Public/cldr/latest
* Extract the downloaded file into a sub-directory of locale/symbolsrc named cldr
* Download UnicodeData.txt: ftp://ftp.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt and put it into the locale/symbolsrc directory
* In locale, type:
```
make import-symbols
```
* When some dictionaries are added, add them to the variable nobase_localedata_DATA in the file Makefile.am located in the "locale" directory.
Use the following syntax to list the new file : `lang/file.dic \` with a tab character before.
Please keep the alphabetical order.

## Dictionaries previously imported from Orca

The `orca.dic`, `orca-chars.dic`, and `orca-math.dic` dictionaries are
retained with their existing translations. They are no longer regenerated
by `make import-symbols`: Orca has removed the character and mathematical
symbol tables that were used by the import script. Installing Orca is
therefore no longer required to import dictionaries.

