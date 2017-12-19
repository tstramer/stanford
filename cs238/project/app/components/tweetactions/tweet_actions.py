from .simple_action import SimpleAction
from .template_action import TemplateAction
from .quote_random_account_tweet import QuoteRandomAccountTweet
from .markov_bot_tweet import MarkovBotTweet
from .with_emoji_action import WithEmojiAction, DEFAULT_EMOJI, POLITICAL_EMOJI
from .list_action import ListAction

INSPIRATIONAL_PREFIXES = ["yep", "right!", "truth", "hmm", "ahem", "interesting", "word"]
NICEPICS_PREFIXES = ["awwww yea", "check this out", "amazing", "rad", "cute", "chill", "perfect", "cool"]
POLITICAL_PREFIXES = ["ummmm", "interesting", "found this", "thoughts?", "not sure what to think of this", "hrmm", "hmm", "read this", "what is this about?", "is this true?", "thoughts?", "not sure what to think of this", "this seems bad"]

ACCOUNTS_TO_QUOTE = [
    ["inspirational", INSPIRATIONAL_PREFIXES, DEFAULT_EMOJI],
    ["nicepics", NICEPICS_PREFIXES, DEFAULT_EMOJI],
    ["political", POLITICAL_PREFIXES, POLITICAL_EMOJI],
]

BOOKS_TO_QUOTE = [
    ["aliceinwonderland", ["Great quote!", "Love this quote!", "Beautiful!"]]
]

CONCERNS = [
    "You doing ok?",
    "How are you today?"
]

COMPLIMENTS = [
    "I bet you make babies smile.",
    "I appreciate you.",
    "You bring out the best in other people.",
    "You're wonderful."
]

ATTACKS = [
    "Disagree!",
    "Proof?",
    "Nope.",
    "Wrong.",
    "False.",
    "Not even close.",
    "Elaborate?"
]

QUOTE_TWEET_ACTIONS = [
    WithEmojiAction(QuoteRandomAccountTweet(account, prefixes), emoji) for (account, prefixes, emoji) in ACCOUNTS_TO_QUOTE
]

MARKOV_BOT_ACTIONS = [
    WithEmojiAction(MarkovBotTweet(book, prefixes=prefixes)) for (book, prefixes) in BOOKS_TO_QUOTE
]

CONCERN_ACTION = WithEmojiAction(ListAction("concern", CONCERNS), DEFAULT_EMOJI)
COMPLIMENT_ACTION = WithEmojiAction(ListAction("compliment", COMPLIMENTS), DEFAULT_EMOJI)
ATTACK_ACTION = WithEmojiAction(ListAction("attack", ATTACKS), POLITICAL_EMOJI)

TWEET_ACTIONS = [CONCERN_ACTION, COMPLIMENT_ACTION, ATTACK_ACTION] + \
                QUOTE_TWEET_ACTIONS + \
                MARKOV_BOT_ACTIONS

