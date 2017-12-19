import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.pluck.source.StatusSource
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{RequiredBinaryComparators, TwitterDateJob}
import java.util.Locale

/**

Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job ExtractSearchQueryTweetCountsJob \
    -- \
    --date 2015-11-01T12 2015-11-07T12 \
    --output searchQueryTweetCounts

To see the data:
  science/scalding/scripts/hdfsget searchQueryTweetCounts
  cat searchQueryTweetCounts
  */
// class ExtractSearchQueryTweetCountsJob(args : Args) extends TwitterDateJob(args) with RequiredBinaryComparators {

//   import ExtractTweetsSampleFromQueryListJob._

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
//         (query, 1)
//       }
//     }
//     .sumByKey
//     .toTypedPipe
//     // write the output to a TypedText.tsv
//     .write(TypedText.tsv[(String, Int)](args("output")))
// }

// object ExtractSearchQueryTweetCountsJob {

//   val SpaceRegex = """\s+"""

//   lazy val tweetLanguageIdentifier = new TwitterLanguageIdentifier.Builder().buildForHighPrecision()

//   def getLang(s: String): String = {
//     tweetLanguageIdentifier.identifyLocale(s.toCharArray).getLanguage
//   }
// }