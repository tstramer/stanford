import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.pluck.source.StatusSource
import com.twitter.pluck.source.combined_user_source.MostRecentCombinedUserSnapshotSource
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{TwitterJob, RequiredBinaryComparators, TwitterDateJob}
import java.util.Locale
import scala.util.Random

/**

Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job SampleTweetsJob \
    -- \
    --date 2015-11-01T12 2015-11-07T12 \
    --output sampleTweets

To see the data:
  science/scalding/scripts/hdfsget sampleTweets
  cat sampleTweets
  */
class SampleTweetsJob(args : Args) extends TwitterDateJob(args) with RequiredBinaryComparators {

  import ExtractTweetsSampleFromQueryListJob._

  val input = TypedText.tsv[TweetRow.Tuple]("/user/tstramer/groupedTweetsSampleWithTweets")

  val tweetRows = TypedPipe.from(input)
    .map(TweetRow.fromTuple)
    .map(tweet =>
      (tweet.userID, Seq(tweet))
    )
    .sumByKey
    .toTypedPipe
    .filter { case (userID, tweets) => tweets.length >= 10 }
    .flatMap { case (userID, tweets) =>
      Random.shuffle(tweets).take(20)
    }
    .write(TypedText.tsv[TweetRow](args("output")))
}