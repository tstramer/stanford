import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{RequiredBinaryComparators, TwitterDateJob}
import com.twitter.roastduck.util.JsonSerializer

/**

Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job GroupTweetsByUserJob \
    -- \
    --date 2015-11-01T12 2015-11-07T12 \
    --output groupTweetsByUser

To see the data:
  science/scalding/scripts/hdfsget groupTweetsByUser
  cat groupTweetsByUser
  */
class GroupTweetsByUserJob(args : Args) extends TwitterDateJob(args) with RequiredBinaryComparators {

  val tweets = TypedText.tsv[TweetRow.Tuple]("/user/tstramer/tweetsSample")

  TypedPipe.from(tweets)
    .map(TweetRow.fromTuple)
    .map { tweet =>
      (tweet.userID, tweet.entities.toSeq)
    }
    .sumByKey
    .toTypedPipe
    .map { case (userID, entities) =>
      val jsonCounts = JsonSerializer(entities.groupBy(identity).mapValues(_.size))
      (userID, jsonCounts)
    }
    // write the output to a TypedText.tsv
    .write(TypedText.tsv[UserRow.Tuple](args("output")))
}