import com.twitter.common.text.pipeline.TwitterLanguageIdentifier
import com.twitter.pluck.source.StatusSource
import com.twitter.scalding._
import com.twitter.scalding.source.TypedText
import com.twitter.scalding.typed.TypedPipe
import com.twitter.scalding_internal.job.{TwitterJob, RequiredBinaryComparators, TwitterDateJob}
import java.util.Locale

/**

Run:
  ./pants compile topics && ./pants bundle topics:topics-deploy
  oscar hdfs --screen \
    --bundle topics-deploy --job ExtractIDFSearchQueryScores \
    -- \
    --output idfSearchQueryScores

To see the data:
  science/scalding/scripts/hdfsget idfSearchQueryScores
  cat idfSearchQueryScores
  */
class ExtractIDFSearchQueryScores(args : Args) extends TwitterJob(args) with RequiredBinaryComparators {

  val input = TypedText.tsv[UserRow.Tuple]("/user/tstramer/usersToQueryCounts")

  TypedPipe.from(input)
    .map(UserRow.fromTuple)
    .flatMap(user =>
      user.counts.keys map { entity =>
        (entity, 1)
      }
    )
    .sumByKey
    .toTypedPipe
    // write the output to a TypedText.tsv
    .write(TypedText.tsv[(String, Int)](args("output")))
}