import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.pluck.source.StatusSource
import com.twitter.scalding.Args
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{TwitterDateJob, RequiredBinaryComparators, TwitterJob}
import java.util.Locale

/**
To run this job:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job CalculateCoOccurrencesJob \
    -- \
    --date 2015-11-01T12 2015-11-07T12 \
    --output coOccurrences

  science/scalding/scripts/hdfsget coOccurrences
  cat coOccurrences

  **/
class CalculateCoOccurrencesJob(args : Args) extends TwitterDateJob(args) with RequiredBinaryComparators {

  import CalculateCoOccurrencesJob._

  val output = TypedText.tsv[CoOccurrenceRow.Tuple](args("output"))

  val englishTweets = TypedPipe.from(StatusSource())
    .filter { status =>
      Option(status.getText).isDefined && status.getText != ""
    }
    .filter { status =>
      getLang(status.getText) == Locale.ENGLISH.getLanguage
    }

  englishTweets
    .flatMap { tweet =>
      val text = Option(tweet.getText).getOrElse("").toLowerCase
      val matchingQueries = SearchQueries.All.filter(text.contains).toSeq
      if (matchingQueries.size < 2) Nil
      else {
        for {
          i <- 0 until matchingQueries.length - 1
          j <- i + 1 until matchingQueries.length
        } yield {
          val less = matchingQueries(i).compareTo(matchingQueries(j))
          if (less <= 0) {
            ((matchingQueries(i), matchingQueries(j)), 1)
          } else {
            ((matchingQueries(j), matchingQueries(i)), 1)
          }
        }
      }
    }
    .sumByKey
    .toTypedPipe
    .filter { case ((_, _), count) => count >= MinCoOccurrences }
    .map { case ((entity1, entity2), count) =>
      (entity1, entity2, count)
    }
    .write(output)
}

object CalculateCoOccurrencesJob {

  val MinCoOccurrences = 10

  lazy val tweetLanguageIdentifier = new TwitterLanguageIdentifier.Builder().buildForHighPrecision()

  def getLang(s: String): String = {
    tweetLanguageIdentifier.identifyLocale(s.toCharArray).getLanguage
  }

}