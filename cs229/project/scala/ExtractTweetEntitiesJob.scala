import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{RequiredBinaryComparators, TwitterDateJob}

/**

Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job ExtractTweetEntitiesJob \
    -- \
    --date 2015-05-25T12 2015-06-25T12 \
    --output tweetEntities

To see the data:
  science/scalding/scripts/hdfsget tweetEntities
  cat tweetEntities
  */
class ExtractTweetEntitiesJob(args : Args) extends TwitterDateJob(args) with RequiredBinaryComparators {

  import ExtractTweetEntitiesJob._

  val input = TypedText.tsv[TweetRow.Tuple]("/user/tstramer/vitTweets1Month")
  val output = TypedText.tsv[(String, Int)](args("output"))

  TypedPipe
    .from(input)
    .flatMap { tuple =>
      val tweetRow = TweetRow.fromTuple(tuple)
      for { entity <- tweetRow.entities } yield {
        (entity, 1)
      }
    }
    .sumByKey
    .toTypedPipe
    .filter { case (entity, count) =>
      count >= MinEntityCount && !StopWords.isStopWords(entity) && entity.length >= 3
    }
    .map { case (entity, count) => (entity, count) }
    .write(output)
}

object ExtractTweetEntitiesJob {

  private val MinEntityCount = 10

}