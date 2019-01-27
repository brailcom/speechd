/*
* spd_cancel_long_message.c - test SPD_CANCEL behaviour
*
 * Copyright (C) 2010 Brailcom, o.p.s.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2, or (at your option)
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <https://www.gnu.org/licenses/>.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "speechd_types.h"
#include "libspeechd.h"

#define TEST_NAME __FILE__
#define TEST_WAIT_COUNT (10)
static int notification_mask;
static SPDConnection *spd;

static const char *events[] = {
	"SPD_EVENT_BEGIN",
	"SPD_EVENT_END",
	"SPD_EVENT_INDEX_MARK",
	"SPD_EVENT_CANCEL",
	"SPD_EVENT_PAUSE",
	"SPD_EVENT_RESUME"
};

/* Callback for Speech Dispatcher notifications */
static void notification_cb(size_t msg_id, size_t client_id,
			    SPDNotificationType type)
{
	notification_mask |= (1 << type);
	printf("notification %s received\n", events[type]);
}

int main(int argc, char *argv[])
{
	int result, count;

	/* Open Speech Dispatcher connection */
	spd = spd_open(TEST_NAME, __FUNCTION__, NULL, SPD_MODE_THREADED);
	if (!spd) {
		printf("Speech-dispatcher: Failed to open connection. \n");
		exit(1);
	}

	printf("Speech-dispatcher: connection opened. \n");

	spd->callback_cancel = notification_cb;

	/* Ask Speech Dispatcher to notify us about these events. */
	result = spd_set_notification_on(spd, SPD_CANCEL);
	if (result == -1) {
		printf("Notification SPD_CANCEL not set correctly \n");
		spd_close(spd);
		exit(1);
	}

	/* The message is spoken as TEXT */
	result = spd_set_data_mode(spd, SPD_DATA_TEXT);
	if (result == -1) {
		printf("Could not set spd_set_data_mode() to SPD_DATA_TEXT \n");
		spd_close(spd);
		exit(1);
	}

	printf("Sending a long message \n");
	result = spd_say(spd, SPD_MESSAGE, ""
			 "						\n"
			 "       ALICE'S ADVENTURES IN WONDERLAND by Lewis Carroll.\n"
			 "\n"
			 "                CHAPTER I: Down the Rabbit-Hole.\n"
			 "\n"
			 "  Alice was beginning to get very tired of sitting by her sister\n"
			 "on the bank, and of having nothing to do:  once or twice she had\n"
			 "peeped into the book her sister was reading, but it had no\n"
			 "pictures or conversations in it, `and what is the use of a book,'\n"
			 "thought Alice `without pictures or conversation?'\n"
			 "\n"
			 "  So she was considering in her own mind (as well as she could,\n"
			 "for the hot day made her feel very sleepy and stupid), whether\n"
			 "the pleasure of making a daisy-chain would be worth the trouble\n"
			 "of getting up and picking the daisies, when suddenly a White\n"
			 "Rabbit with pink eyes ran close by her.\n"
			 "\n"
			 "  There was nothing so VERY remarkable in that; nor did Alice\n"
			 "think it so VERY much out of the way to hear the Rabbit say to\n"
			 "itself, `Oh dear!  Oh dear!  I shall be late!'  (when she thought\n"
			 "it over afterwards, it occurred to her that she ought to have\n"
			 "wondered at this, but at the time it all seemed quite natural);\n"
			 "but when the Rabbit actually TOOK A WATCH OUT OF ITS WAISTCOAT-\n"
			 "POCKET, and looked at it, and then hurried on, Alice started to\n"
			 "her feet, for it flashed across her mind that she had never\n"
			 "before seen a rabbit with either a waistcoat-pocket, or a watch to\n"
			 "take out of it, and burning with curiosity, she ran across the\n"
			 "field after it, and fortunately was just in time to see it pop\n"
			 "down a large rabbit-hole under the hedge.\n"
			 "\n"
			 "  In another moment down went Alice after it, never once\n"
			 "considering how in the world she was to get out again.\n"
			 "\n"
			 "  The rabbit-hole went straight on like a tunnel for some way,\n"
			 "and then dipped suddenly down, so suddenly that Alice had not a\n"
			 "moment to think about stopping herself before she found herself\n"
			 "falling down a very deep well.\n"
			 "\n"
			 "  Either the well was very deep, or she fell very slowly, for she\n"
			 "had plenty of time as she went down to look about her and to\n"
			 "wonder what was going to happen next.  First, she tried to look\n"
			 "down and make out what she was coming to, but it was too dark to\n"
			 "see anything; then she looked at the sides of the well, and\n"
			 "noticed that they were filled with cupboards and book-shelves;\n"
			 "here and there she saw maps and pictures hung upon pegs.  She\n"
			 "took down a jar from one of the shelves as she passed; it was\n"
			 "labelled `ORANGE MARMALADE', but to her great disappointment it\n"
			 "was empty:  she did not like to drop the jar for fear of killing\n"
			 "somebody, so managed to put it into one of the cupboards as she\n"
			 "fell past it.\n"
			 "\n"
			 "  `Well!' thought Alice to herself, `after such a fall as this, I\n"
			 "shall think nothing of tumbling down stairs!  How brave they'll\n"
			 "all think me at home!  Why, I wouldn't say anything about it,\n"
			 "even if I fell off the top of the house!' (Which was very likely\n"
			 "true.)\n"
			 "\n"
			 "  Down, down, down.  Would the fall NEVER come to an end!  `I\n"
			 "wonder how many miles I've fallen by this time?' she said aloud.\n"
			 "`I must be getting somewhere near the centre of the earth.  Let\n"
			 "me see:  that would be four thousand miles down, I think--' (for,\n"
			 "you see, Alice had learnt several things of this sort in her\n"
			 "lessons in the schoolroom, and though this was not a VERY good\n"
			 "opportunity for showing off her knowledge, as there was no one to\n"
			 "listen to her, still it was good practice to say it over) `--yes,\n"
			 "that's about the right distance--but then I wonder what Latitude\n"
			 "or Longitude I've got to?'  (Alice had no idea what Latitude was,\n"
			 "or Longitude either, but thought they were nice grand words to\n"
			 "say.)\n"
			 "\n"
			 "  Presently she began again.  `I wonder if I shall fall right\n"
			 "THROUGH the earth!  How funny it'll seem to come out among the\n"
			 "people that walk with their heads downward!  The Antipathies, I\n"
			 "think--' (she was rather glad there WAS no one listening, this\n"
			 "time, as it didn't sound at all the right word) `--but I shall\n"
			 "have to ask them what the name of the country is, you know.\n"
			 "Please, Ma'am, is this New Zealand or Australia?' (and she tried\n"
			 "to curtsey as she spoke--fancy CURTSEYING as you're falling\n"
			 "through the air!  Do you think you could manage it?)  `And what\n"
			 "an ignorant little girl she'll think me for asking!  No, it'll\n"
			 "never do to ask:  perhaps I shall see it written up somewhere.'\n"
			 "\n"
			 "  Down, down, down.  There was nothing else to do, so Alice soon\n"
			 "began talking again.  `Dinah'll miss me very much to-night, I\n"
			 "should think!'  (Dinah was the cat.)  `I hope they'll remember\n"
			 "her saucer of milk at tea-time.  Dinah my dear!  I wish you were\n"
			 "down here with me!  There are no mice in the air, I'm afraid, but\n"
			 "you might catch a bat, and that's very like a mouse, you know.\n"
			 "But do cats eat bats, I wonder?'  And here Alice began to get\n"
			 "rather sleepy, and went on saying to herself, in a dreamy sort of\n"
			 "way, `Do cats eat bats?  Do cats eat bats?' and sometimes, `Do\n"
			 "bats eat cats?' for, you see, as she couldn't answer either\n"
			 "question, it didn't much matter which way she put it.  She felt\n"
			 "that she was dozing off, and had just begun to dream that she\n"
			 "was walking hand in hand with Dinah, and saying to her very\n"
			 "earnestly, `Now, Dinah, tell me the truth:  did you ever eat a\n"
			 "bat?' when suddenly, thump! thump! down she came upon a heap of\n"
			 "sticks and dry leaves, and the fall was over.\n"
			 "\n"
			 "  Alice was not a bit hurt, and she jumped up on to her feet in a\n"
			 "moment:  she looked up, but it was all dark overhead; before her\n"
			 "was another long passage, and the White Rabbit was still in\n"
			 "sight, hurrying down it.  There was not a moment to be lost:\n"
			 "away went Alice like the wind, and was just in time to hear it\n"
			 "say, as it turned a corner, `Oh my ears and whiskers, how late\n"
			 "it's getting!'  She was close behind it when she turned the\n"
			 "corner, but the Rabbit was no longer to be seen:  she found\n"
			 "herself in a long, low hall, which was lit up by a row of lamps\n"
			 "hanging from the roof.\n"
			 "\n"
			 "  There were doors all round the hall, but they were all locked;\n"
			 "and when Alice had been all the way down one side and up the\n"
			 "other, trying every door, she walked sadly down the middle,\n"
			 "wondering how she was ever to get out again.\n"
			 "\n"
			 "  Suddenly she came upon a little three-legged table, all made of\n"
			 "solid glass; there was nothing on it except a tiny golden key,\n"
			 "and Alice's first thought was that it might belong to one of the\n"
			 "doors of the hall; but, alas! either the locks were too large, or\n"
			 "the key was too small, but at any rate it would not open any of\n"
			 "them.  However, on the second time round, she came upon a low\n"
			 "curtain she had not noticed before, and behind it was a little\n"
			 "door about fifteen inches high:  she tried the little golden key\n"
			 "in the lock, and to her great delight it fitted!\n"
			 "\n"
			 "  Alice opened the door and found that it led into a small\n"
			 "passage, not much larger than a rat-hole:  she knelt down and\n"
			 "looked along the passage into the loveliest garden you ever saw.\n"
			 "How she longed to get out of that dark hall, and wander about\n"
			 "among those beds of bright flowers and those cool fountains, but\n"
			 "she could not even get her head though the doorway; `and even if\n"
			 "my head would go through,' thought poor Alice, `it would be of\n"
			 "very little use without my shoulders.  Oh, how I wish\n"
			 "I could shut up like a telescope!  I think I could, if I only\n"
			 "know how to begin.'  For, you see, so many out-of-the-way things\n"
			 "had happened lately, that Alice had begun to think that very few\n"
			 "things indeed were really impossible.\n"
			 "\n"
			 "  There seemed to be no use in waiting by the little door, so she\n"
			 "went back to the table, half hoping she might find another key on\n"
			 "it, or at any rate a book of rules for shutting people up like\n"
			 "telescopes:  this time she found a little bottle on it, (`which\n"
			 "certainly was not here before,' said Alice,) and round the neck\n"
			 "of the bottle was a paper label, with the words `DRINK ME'\n"
			 "beautifully printed on it in large letters.\n"
			 "\n"
			 "  It was all very well to say `Drink me,' but the wise little\n"
			 "Alice was not going to do THAT in a hurry.  `No, I'll look\n"
			 "first,' she said, `and see whether it's marked \"poison\" or not';\n"
			 "for she had read several nice little histories about children who\n"
			 "had got burnt, and eaten up by wild beasts and other unpleasant\n"
			 "things, all because they WOULD not remember the simple rules\n"
			 "their friends had taught them:  such as, that a red-hot poker\n"
			 "will burn you if you hold it too long; and that if you cut your\n"
			 "finger VERY deeply with a knife, it usually bleeds; and she had\n"
			 "never forgotten that, if you drink much from a bottle marked\n"
			 "`poison,' it is almost certain to disagree with you, sooner or\n"
			 "later.\n"
			 "\n"
			 "  However, this bottle was NOT marked `poison,' so Alice ventured\n"
			 "to taste it, and finding it very nice, (it had, in fact, a sort\n"
			 "of mixed flavour of cherry-tart, custard, pine-apple, roast\n"
			 "turkey, toffee, and hot buttered toast,) she very soon finished\n"
			 "it off.\n"
			 "\n"
			 "  `What a curious feeling!' said Alice; `I must be shutting up\n"
			 "like a telescope.'\n"
			 "\n"
			 "  And so it was indeed:  she was now only ten inches high, and\n"
			 "her face brightened up at the thought that she was now the right\n"
			 "size for going though the little door into that lovely garden.\n"
			 "First, however, she waited for a few minutes to see if she was\n"
			 "going to shrink any further:  she felt a little nervous about\n"
			 "this; `for it might end, you know,' said Alice to herself, `in my\n"
			 "going out altogether, like a candle.  I wonder what I should be\n"
			 "like then?'  And she tried to fancy what the flame of a candle is\n"
			 "like after the candle is blown out, for she could not remember\n"
			 "ever having seen such a thing.\n"
			 "\n"
			 "  After a while, finding that nothing more happened, she decided\n"
			 "on going into the garden at once; but, alas for poor Alice! when\n"
			 "she got to the door, she found he had forgotten the little golden\n"
			 "key, and when she went back to the table for it, she found she\n"
			 "could not possibly reach it:  she could see it quite plainly\n"
			 "through the glass, and she tried her best to climb up one of the\n"
			 "legs of the table, but it was too slippery; and when she had\n"
			 "tired herself out with trying, the poor little thing sat down and\n"
			 "cried.\n"
			 "\n"
			 "  `Come, there's no use in crying like that!' said Alice to\n"
			 "herself, rather sharply; `I advise you to leave off this minute!'\n"
			 "She generally gave herself very good advice, (though she very\n"
			 "seldom followed it), and sometimes she scolded herself so\n"
			 "severely as to bring tears into her eyes; and once she remembered\n"
			 "trying to box her own ears for having cheated herself in a game\n"
			 "of croquet she was playing against herself, for this curious\n"
			 "child was very fond of pretending to be two people.  `But it's no\n"
			 "use now,' thought poor Alice, `to pretend to be two people!  Why,\n"
			 "there's hardly enough of me left to make ONE respectable\n"
			 "person!'\n"
			 "\n"
			 "  Soon her eye fell on a little glass box that was lying under\n"
			 "the table:  she opened it, and found in it a very small cake, on\n"
			 "which the words `EAT ME' were beautifully marked in currants.\n"
			 "`Well, I'll eat it,' said Alice, `and if it makes me grow larger,\n"
			 "I can reach the key; and if it makes me grow smaller, I can creep\n"
			 "under the door; so either way I'll get into the garden, and I\n"
			 "don't care which happens!'\n"
			 "\n"
			 "  She ate a little bit, and said anxiously to herself, `Which\n"
			 "way?  Which way?', holding her hand on the top of her head to\n"
			 "feel which way it was growing, and she was quite surprised to\n"
			 "find that she remained the same size:  to be sure, this generally\n"
			 "happens when one eats cake, but Alice had got so much into the\n"
			 "way of expecting nothing but out-of-the-way things to happen,\n"
			 "that it seemed quite dull and stupid for life to go on in the\n"
			 "common way.\n"
			 "\n"
			 "  So she set to work, and very soon finished off the cake. ");

	if (result == -1) {
		printf("spd_say() failed. \n");
		spd_close(spd);
		exit(1);
	}

	printf("Wait 5 secs and then cancel it.\n");
	sleep(5);
	result = spd_cancel(spd);
	if (result == -1) {
		printf("spd_cancel() failed. \n");
		spd_close(spd);
		exit(1);
	}

	count = 0;
	while (~notification_mask & SPD_CANCEL) {
		sleep(1);
		if (count++ == TEST_WAIT_COUNT) {
			printf("SPD_CANCEL wait count exceeded\n");
			spd_close(spd);
			exit(1);
		}
	}

	printf("Message successfuly canceled.\n");

	printf("Speech-dispatcher: connection closed.\n");

	exit(0);
}
