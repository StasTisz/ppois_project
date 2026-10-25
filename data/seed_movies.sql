-- Seed script for SQLite Database: movies table
-- Contains iconic movies for initial cold-start and genre-based recommendations

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS movies (
    id INTEGER PRIMARY KEY,
    title TEXT NOT NULL,
    year INTEGER,
    genres TEXT,
    director TEXT,
    description TEXT,
    rating_avg REAL,
    poster_url TEXT
);

CREATE INDEX IF NOT EXISTS idx_movies_rating ON movies(rating_avg DESC);

INSERT OR REPLACE INTO movies (id, title, year, genres, director, description, rating_avg, poster_url) VALUES
(1, 'The Shawshank Redemption', 1994, 'Drama', 'Frank Darabont', 'Two imprisoned men bond over a number of years, finding solace and eventual redemption through acts of common decency.', 9.3, 'https://m.media-amazon.com/images/M/MV5BMDFkYTc0MGEtZmNhMC00ZDIzLWFmNTEtODM1ZmRlYWMwMWFmXkEyXkFqcGdeQXVyMTMxODk2OTU@._V1_.jpg'),
(2, 'The Godfather', 1972, 'Crime, Drama', 'Francis Ford Coppola', 'The aging patriarch of an organized crime dynasty transfers control of his clandestine empire to his reluctant son.', 9.2, 'https://m.media-amazon.com/images/M/MV5BM2MyNjYxNmUtYTAwNi00MTYxLWJmNWYtYzZlODY3ZTk3OTFlXkEyXkFqcGdeQXVyNzkwMjQ5NzEt._V1_.jpg'),
(3, 'The Dark Knight', 2008, 'Action, Crime, Drama', 'Christopher Nolan', 'When the menace known as the Joker wreaks havoc and chaos on the people of Gotham, Batman must accept one of the greatest psychological and physical tests of his ability to fight injustice.', 9.0, 'https://m.media-amazon.com/images/M/MV5BMTMxNTMwODM0NF5BMl5BanBnXkFtZTcwODAyMTk2Mw@@._V1_.jpg'),
(4, 'Pulp Fiction', 1994, 'Crime, Drama', 'Quentin Tarantino', 'The lives of two mob hitmen, a boxer, a gangster and his wife, and a pair of diner bandits intertwine in four tales of violence and redemption.', 8.9, 'https://m.media-amazon.com/images/M/MV5BNGNhMDIzZTUtNTBlZi00MTRlLWFjM2ItYzViMjE3YzI5MjljXkEyXkFqcGdeQXVyNzkwMjQ5NzEt._V1_.jpg'),
(5, 'Inception', 2010, 'Action, Adventure, Sci-Fi', 'Christopher Nolan', 'A thief who steals corporate secrets through the use of dream-sharing technology is given the inverse task of planting an idea into the mind of a C.E.O.', 8.8, 'https://m.media-amazon.com/images/M/MV5BMjAxMzY3NjcxNF5BMl5BanBnXkFtZTcwNTI5OTM0Mw@@._V1_.jpg'),
(6, 'Fight Club', 1999, 'Drama', 'David Fincher', 'An insomniac office worker and a devil-may-care soap maker form an underground fight club that evolves into much more.', 8.8, 'https://m.media-amazon.com/images/M/MV5BODQ0OWJiMzktYjNlYi00Uw_4LWI3OWQtYmJlZDJiNzBkZGY3XkEyXkFqcGdeQXVyNzkwMjQ5NzEt._V1_.jpg'),
(7, 'Forrest Gump', 1994, 'Drama, Romance', 'Robert Zemeckis', 'The history of the United States from the 1950s to the 70s unfolds from the perspective of an Alabama man with an IQ of 75.', 8.8, 'https://m.media-amazon.com/images/M/MV5BNWIwODRlZTUtY2U3ZS00Yzg1LWJhNzYtMmZiYmEyNmU1NjMzXkEyXkFqcGdeQXVyMTQxNzMzNDI@._V1_.jpg'),
(8, 'The Matrix', 1999, 'Action, Sci-Fi', 'Lana Wachowski, Lilly Wachowski', 'When a beautiful stranger leads computer hacker Neo to a forbidding underworld, he discovers the shocking truth--the life he knows is the elaborate deception of an evil cyber-intelligence.', 8.7, 'https://m.media-amazon.com/images/M/MV5BNzQzOTk3OTAtNDQ0Zi00ZTVkLWI0MTEtMDllZjNkYzNjNTc4L2ltYWdlXkEyXkFqcGdeQXVyNjU0OTQ0OTY@._V1_.jpg'),
(9, 'Goodfellas', 1990, 'Biography, Crime, Drama', 'Martin Scorsese', 'The story of Henry Hill and his life in the mafia, covering his relationship with his wife Karen and his mob partners Jimmy Conway and Tommy DeVito.', 8.7, 'https://m.media-amazon.com/images/M/MV5BY2NkZjEzMDgtN2RjYy00YzM1LWI4ZmUtMjIwYmM0NmI4NWQ3XkEyXkFqcGdeQXVyNzkwMjQ5NzEt._V1_.jpg'),
(10, 'Interstellar', 2014, 'Adventure, Drama, Sci-Fi', 'Christopher Nolan', 'When Earth becomes uninhabitable in the future, a farmer and ex-NASA pilot, Joseph Cooper, is tasked to pilot a spacecraft, along with a team of researchers, to find a new planet for humans.', 8.7, 'https://m.media-amazon.com/images/M/MV5BZjdkOTU3MDktN2IxOS00OGEyLWFmMjktY2FiMmZkNWIyODZiXkEyXkFqcGdeQXVyMTMxODk2OTU@._V1_.jpg'),
(11, 'Spirited Away', 2001, 'Animation, Adventure, Fantasy', 'Hayao Miyazaki', 'During her family move to the suburbs, a sullen 10-year-old girl wanders into a world ruled by gods, witches and spirits.', 8.6, 'https://m.media-amazon.com/images/M/MV5BMjlmZmI5MDctNDE2YS00YWE5LWE5ZWItZDBhYWQ0NTcxNWRhXkEyXkFqcGdeQXVyMTMxODk2OTU@._V1_.jpg'),
(12, 'Parasite', 2019, 'Drama, Thriller', 'Bong Joon Ho', 'Greed and class discrimination threaten the newly formed symbiotic relationship between the wealthy Park family and the destitute Kim clan.', 8.5, 'https://m.media-amazon.com/images/M/MV5BYWZjMjk3ZTItODQ2ZC00NTY5LWE0ZDYtZTI3MjcwN2Q5NTVkXkEyXkFqcGdeQXVyODk4OTc3MTY@._V1_.jpg'),
(13, 'The Green Mile', 1999, 'Crime, Drama, Fantasy', 'Frank Darabont', 'A tale set on death row in a Southern jail, where gentle giant John Coffey possesses the mysterious power to heal people.', 8.6, 'https://m.media-amazon.com/images/M/MV5BMTUxMzQyNjA5MF5BMl5BanBnXkFtZTYwOTU2NTY3._V1_.jpg'),
(14, 'Gladiator', 2000, 'Action, Adventure, Drama', 'Ridley Scott', 'A former Roman General sets out to exact vengeance against the corrupt emperor who murdered his family and sent him into slavery.', 8.5, 'https://m.media-amazon.com/images/M/MV5BMDliOTIzNmUtNzVlMS00NjU5LWFjZDMtOTAwZWFjMWZmYjgxXkEyXkFqcGdeQXVyNTIzOTk5ODM@._V1_.jpg'),
(15, 'The Lion King', 1994, 'Animation, Adventure, Drama', 'Roger Allers, Rob Minkoff', 'Lion prince Simba and his father are targeted by his bitter uncle, who wants to ascend the throne himself.', 8.5, 'https://m.media-amazon.com/images/M/MV5BYTYxNGMyZTYtMjE3MS00MzNjLWFjNmUtMDFlVEDjZWExOWFmXkEyXkFqcGdeQXVyNjY5NDU4NzI@._V1_.jpg'),
(16, 'The Departed', 2006, 'Crime, Drama, Thriller', 'Martin Scorsese', 'An undercover cop and a mole in the police attempt to identify each other while infiltrating an Irish gang in South Boston.', 8.5, 'https://m.media-amazon.com/images/M/MV5BMTI1MTY2OTIxNV5BMl5BanBnXkFtZTYwNjQ4NjY3._V1_.jpg'),
(17, 'Whiplash', 2014, 'Drama, Music', 'Damien Chazelle', 'A promising young drummer enrolls at a cut-throat music conservatory where his dreams of greatness are mentored by an instructor who will stop at nothing.', 8.5, 'https://m.media-amazon.com/images/M/MV5BOTA5NDZlZGUtMjAxOS00YTRkLTkwYmMtYWQ0NWEwZDc4YzRjXkEyXkFqcGdeQXVyNzkwMjQ5NzEt._V1_.jpg'),
(18, 'The Prestige', 2006, 'Drama, Mystery, Sci-Fi', 'Christopher Nolan', 'After a tragic accident, two stage magicians in 1890s London engage in a battle to create the ultimate illusion.', 8.5, 'https://m.media-amazon.com/images/M/MV5BMjA4NDI0MTIxNF5BMl5BanBnXkFtZTYwNTM0MzY2._V1_.jpg'),
(19, 'Alien', 1979, 'Horror, Sci-Fi', 'Ridley Scott', 'The crew of a commercial spacecraft encounters a deadly lifeform after investigating a mysterious transmission of unknown origin.', 8.5, 'https://m.media-amazon.com/images/M/MV5BOGQzZTBjMjQtOTFiMS00N2EzLWEzYTgtMmUxNzkxNGVkYmQ4XkEyXkFqcGdeQXVyOTA1ODk2NzA@._V1_.jpg'),
(20, 'Psycho', 1960, 'Horror, Mystery, Thriller', 'Alfred Hitchcock', 'A Phoenix secretary embezzles $40,000 from her employer client, goes on the run and checks into a remote motel.', 8.5, 'https://m.media-amazon.com/images/M/MV5BNTQwNDM1YzItNDAxZC00NWY2LTk0M2UtNDExNWE2NDIxNzYxXkEyXkFqcGdeQXVyMTQxNzMzNDI@._V1_.jpg');
