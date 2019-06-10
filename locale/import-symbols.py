#!/usr/bin/env python3.5
# coding: utf-8
# NVDA symbols and cldr import
# Import NVDA symbols dictionaries then emojies from CLDR database
# Copyright (C) 2001, 2019 Brailcom, o.p.s.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
###
#This file includes portions of code from the NVDA project.
#URL: http://www.nvda-project.org/
###

from sys import argv as arguments, stderr
from os import path
import codecs
from collections import OrderedDict
import csv

class SymbolsDictionary(OrderedDict):
	""" This class inherited from OrderedDict is displayed as a .dic file content when converted to a string
	Attribute: symbolsQualifier, default None - if the 4th column must have one """

	def __init__(self, symbolsQualifier=None,*args,**kwargs):
		super(SymbolsDictionary, self).__init__(*args,**kwargs)
		self.symbolsQualifier=symbolsQualifier

	def __str__(self):
		dicText = str("symbols:\r\n")
		for pattern, description in self.items():
			if self.symbolsQualifier:
				dicText = str().join((dicText,u"{pattern}\t{description}\tnone\t{qualifier}\r\n".format(
				pattern=pattern,
				description=description,
				qualifier=self.symbolsQualifier
				)))
			else:
				dicText = str().join((dicText,u"{pattern}\t{description}\tnone\r\n".format(
				pattern=pattern,
				description=description
				)))
		return dicText

def createCLDRAnnotationsDict(sources):
	""" Argument: the list of CLDR files to parse
	Returns: a SymbolsDictionary object containing all emojies in the specified language """
	from xml.etree import ElementTree
	cldrDict = SymbolsDictionary() # Where the list of emojies will be storred
	# We brows the source files
	for source in sources:
		tree = ElementTree.parse(source) # We parse the CLDR XML file to extract all emojies
		for element in tree.iter("annotation"):
			if element.attrib.get("type") == "tts":
				cldrDict[element.attrib['cp']] = element.text.replace(":","")
	assert cldrDict, "cldrDict is empty"
	return cldrDict

# The list of languages supported by Speech-Dispatcher and the corresponding language(s) in the CLDR database
# For language where there are no emojies, we put an empty list
speechdToCLDRLocales = {
	"an":(),
	"ar":("ar",),
	"bg":("bg",),
	"ca":("ca",),
	"cs":("cs",),
	"da":("da",),
	"de":("de",),
	"el":("el",),
	"en":("en_001","en"),
	"es":("es",),
	"es_CO":(),
	"fa":("fa",),
	"fi":("fi",),
	"fr":("fr",),
	"ga":("ga",),
	"gl":("gl",),
	"he":("he",),
	"hi":("hi",),
	"hr":("hr",),
	"hu":("hu",),
	"is":("is",),
	"it":("it",),
	"ja":("ja",),
	"ka":("ka",),
	"kn":("kn",),
	"ko":("ko",),
	"lt":("lt",),
	"my":("my",),
	"nb_NO":("nb",),
	"nl":("nl",),
	"nn_NO":("nn",),
	"pa":("pa",),
	"pl":("pl",),
	"pt_BR":("pt",),
	"pt_PT":("pt","pt_PT"),
	"ro":("ro",),
	"ru":("ru",),
	"sk":("sk",),
	"sl":("sl",),
	"sq":("sq",),
	"sr":("sr",),
	"sv":("sv",),
	"ta":("ta",),
	"tr":("tr",),
	"uk":("uk",),
	"vi":("vi",),
	"zh":("zh",),
	"zh_CN":("zh",),
	"zh_HK":("zh","zh_Hant_HK"),
	"zh_TW":("zh","zh_Hant"),
}

NVDADir = "symbolsrc/beta"
CLDRDir = "symbolsrc/cldr"
unicodeDataPath = "symbolsrc/UnicodeData.txt"

# This is the header which will be inserted in all symbols file
docHeader = """# This file is generated automatically by the script import-symbols.py during make
# To configure it:
# in the locale directory of Speech-Dispatcher source tree
# Download NVDA sources using this link: https://github.com/nvaccess/nvda/archive/beta.zip
# Extract the archive in the newly created directory which will create a nvda-beta folder
# Download the latest unicode CLDR release from this webpage: http://cldr.unicode.org/index/downloads
# In the table, chose the latest release which has a date specified and click on the "data" link
# Click on "core.zip and download it, then extract it into a sub-directory of symbolsrc named cldr
# Download UnicodeData.txt: ftp://ftp.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt and put it into the symbolsrc directory
# In locale, type:
# make import-symbols

"""

# Open UnicodeData.txt passed as 3rd command line argument, parse it and put all font variants to a SymboleDictionnary object
with open(unicodeDataPath, "r", newline='') as uDataFile:
	uDataCSV = csv.reader(uDataFile, delimiter = ';')
	fontVariantsDict = SymbolsDictionary("literal")
	for uCharacter in uDataCSV:
		# The 6th field starts with <font> code for all font variants
		# So get the code after <font> and the first field content to have the character and the one to pronounce
		if uCharacter[5].startswith("<font>"):
			fontVariant = chr(int("0x"+uCharacter[0], 16))
			pronouncedCharacter = chr(int("0x"+uCharacter[5][7:], 16))
			fontVariantsDict[fontVariant] = pronouncedCharacter

# Write the dictionary to en/font-variants.dic with documentation at the top
with codecs.open("en/font-variants.dic", "w", "utf_8_sig", errors="replace") as dicFile:
	dicFile.write(docHeader)
	dicFile.write(str(fontVariantsDict))

annotationsDir = path.join(CLDRDir, "common/annotations")
annotationsDerivedDir = path.join(CLDRDir, "common/annotationsDerived")


# Firstly, check for a NVDA symbols dictionnary to import
# secondly, add all annotations, then the derived ones.
for destLocale, sourceLocales in speechdToCLDRLocales.items():
	# NVDA symbols dictionary import
	if path.exists(path.join(NVDADir, "source/locale", destLocale, "symbols.dic")):
		NVDADic = codecs.open("%s/source/locale/%s/symbols.dic"%(NVDADir,destLocale),"r", "utf_8_sig").read()
		# We write the dictionnary with the doc and the punctuations
		with codecs.open(path.join(destLocale, "symbols.dic"), "w", "utf_8_sig", errors="replace") as dicFile:
			dicFile.write(docHeader)
			dicFile.write(NVDADic)

	# Emojies import
	if len(sourceLocales) == 0:
		continue
	cldrSources = []
	# First add all annotations, then the derived ones.
	for sourceLocale in sourceLocales:
		cldrSources.append("%s/%s.xml" % (annotationsDir,sourceLocale))
		cldrSources.append("%s/%s.xml" % (annotationsDerivedDir,sourceLocale))
	emojiDic = createCLDRAnnotationsDict(cldrSources)
	# We write the dictionnary with the doc and the emojies
	with codecs.open(path.join(destLocale, "emojis.dic"), "w", "utf_8_sig", errors="replace") as dicFile:
		dicFile.write(docHeader)
		dicFile.write(str(emojiDic))
