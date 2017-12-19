import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.common_internal.text.pipeline.TwitterPhraseExtractor
import com.twitter.common_internal.text.version.PenguinVersion
import com.twitter.pluck.source.StatusSource
import com.twitter.pluck.source.combined_user_source.MostRecentCombinedUserSnapshotSource
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding_internal.job.{RequiredBinaryComparators, TwitterDateJob}
import java.util.Locale
import scala.collection.JavaConverters._
import scala.util.Random

/**
Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job ExtractTweetsJob \
    -- \
    --date 2015-05-25T12 2015-06-25T12 \
    --output tweets

To see the data:
  science/scalding/scripts/hdfsget tweets
  cat tweets
*/
// class ExtractTweetsJob(args : Args) extends TwitterDateJob(args) with RequiredBinaryComparators {

//   import ExtractTweetsJob._

//   val filteredUsers = TypedPipe.from(MostRecentCombinedUserSnapshotSource)
//     // Sanity check on necessary fields.
//     .filter{ case user =>
//     user.isSetUser && user.getUser.isSetId && user.getUser.getId > 0L &&
//       user.isSetUser_extended && user.getUser_extended.isSetTweepcred2 && user.getUser_extended.isSetFollowers }

//   val verifiedUsers = filteredUsers
//     .filter { user =>
//       (for {
//         user <- Option(user.getUser)
//         safety <- Option(user.getSafety)
//       } yield safety.verified && !safety.is_protected).getOrElse(false)
//     }
//     .map(user => (user.getUser.getId, user))

//   val sampleEnglishTweets = TypedPipe.from(StatusSource())
//     .filter { status =>
//       Option(status.getText).isDefined && status.getText != ""
//     }
//     .filter { status =>
//       getLang(status.getText) == Locale.ENGLISH.getLanguage
//     }
//     .filter { status =>
//       Random.nextInt(SampleRate) == 0
//     }
//     .map(status => status.getUserId -> status)

//   sampleEnglishTweets
//     .group
//     .join(verifiedUsers)
//     .toTypedPipe
//     .flatMap { case (userId, (status, user)) =>
//       val phrases = getPhrases(status.getText) filter { phrase =>
//         phrase.split(SpaceRegex).size <= MaxPhraseTokens && !phrase.contains('\n')
//       }
//       if (phrases.nonEmpty && phrases.mkString("").trim.nonEmpty) {
//         Some(
//           (
//             userId,
//             status.getId,
//             user.getUser_extended.getFollowers.toLong,
//             user.getUser_extended.getTweepcred2.toDouble,
//             phrases.mkString(TweetRow.EntityDelimiter)
//           )
//         )
//       } else None
//     }
//     // write the output to a TypedText.tsv
//     .write(TypedText.tsv[TweetRow.Tuple](args("output")))
// }

// object ExtractTweetsJob {

//   val SpaceRegex = """\s+"""

//   val SampleRate = 150

//   val MaxPhraseTokens = 3

//   lazy val twitterPhraseExtractor = new TwitterPhraseExtractor.Builder(PenguinVersion.getLatest).build()

//   lazy val tweetLanguageIdentifier = new TwitterLanguageIdentifier.Builder().buildForHighPrecision()

//   def getPhrases(s: String): List[String] = {
//     twitterPhraseExtractor.extractPhrasesAsString(s.toCharArray, Locale.ENGLISH).asScala.toList
//   }

//   def getLang(s: String): String = {
//     tweetLanguageIdentifier.identifyLocale(s.toCharArray).getLanguage
//   }
// }