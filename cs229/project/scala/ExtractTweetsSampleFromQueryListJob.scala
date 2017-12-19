import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.pluck.source.StatusSource
import com.twitter.pluck.source.combined_user_source.MostRecentCombinedUserSnapshotSource
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{RequiredBinaryComparators, TwitterDateJob}
import java.util.Locale
import scala.util.Random

/**

Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job ExtractTweetsSampleFromQueryListJob \
    -- \
    --date 2015-11-01T12 2015-11-07T12 \
    --output tweetsSampleFromQueryList

To see the data:
  science/scalding/scripts/hdfsget tweetsSampleFromQueryList
  cat tweetsSampleFromQueryList
  */
// class ExtractTweetsSampleFromQueryListJob(args : Args) extends TwitterDateJob(args) with RequiredBinaryComparators {

//   import ExtractTweetsSampleFromQueryListJob._

//   val filteredUsers = TypedPipe.from(MostRecentCombinedUserSnapshotSource)
//     // Sanity check on necessary fields.
//     .filter { case user =>
//       user.isSetUser && user.getUser.isSetId && user.getUser.getId > 0L &&
//         user.isSetUser_extended && user.getUser_extended.isSetTweepcred2 && user.getUser_extended.isSetFollowers
//     }

//   val verifiedUsers = filteredUsers
//     .filter { user =>
//       (for {
//         user <- Option(user.getUser)
//         safety <- Option(user.getSafety)
//       } yield safety.verified && !safety.is_protected).getOrElse(false)
//     }
//     .map(user => (user.getUser.getId, user))

//   val tweetsMatchingQuery = TypedPipe.from(StatusSource())
//     .filter { status =>
//       Option(status.getText).isDefined && status.getText != ""
//     }
//     .filter { status =>
//       getLang(status.getText) == Locale.ENGLISH.getLanguage
//     }
//     .flatMap { status =>
//       val text = Option(status.getText).getOrElse("").toLowerCase
//       SearchQueries.All.filter(text.contains).toSeq map { query =>
//         (status.getUserId, (query, status))
//       }
//     }

//   tweetsMatchingQuery
//     .group
//     .join(verifiedUsers)
//     .toTypedPipe
//     .map { case (userId, ((query, status), user)) =>
//       (
//         userId,
//         status.getId,
//         user.getUser_extended.getFollowers.toLong,
//         user.getUser_extended.getTweepcred2.toDouble,
//         query
//       )
//     }
//     // write the output to a TypedText.tsv
//     .write(TypedText.tsv[TweetRow.Tuple](args("output")))
// }

object ExtractTweetsSampleFromQueryListJob {

  val SpaceRegex = """\s+"""

  lazy val tweetLanguageIdentifier = new TwitterLanguageIdentifier.Builder().buildForHighPrecision()

  def getLang(s: String): String = {
    tweetLanguageIdentifier.identifyLocale(s.toCharArray).getLanguage
  }
}